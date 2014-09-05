#include "worker_pool.h" 

#include "log.h"
#include "contextqueue.h"
#include "atomic_ops.h"
#include "ctx_mgr.h"
#include "define.h"
#include "message.h"
#include "messagequeue.h"
#include "zmalloc.h"
#include "thread.h"

#include <assert.h>
#include <stdio.h>
#include <pthread.h>

struct workerPool {
	int threadNum;
	int sleepThreadNum;
	int running;
	struct thread **threads;
	struct contextQueue *queue;
	pthread_mutex_t lock;
	pthread_cond_t cond;
};

static struct workerPool *M = NULL;

static bool workerPoolHaveWork() {
	if (contextQueueSize(M->queue) > 0) {
		return true;
	}
	return false;
}


static void threadSleep() {
	pthread_mutex_lock(&M->lock);
	while(!workerPoolHaveWork() && M->running) {
		pthread_cond_wait(&M->cond, &M->lock);
	}
	pthread_mutex_unlock(&M->lock);
}

static void threadWakeUp() {
	pthread_mutex_lock(&M->lock);
	pthread_cond_signal(&M->cond);
	pthread_mutex_unlock(&M->lock);
}

static void *handle(void *ud) {
	struct thread *th = (struct thread*)ud;
	while (true) {
		struct context *ctx = contextQueuePop(M->queue); 
		bool sleep = false;
		if (ctx) {
			do {
				int result = contextDispatchMessage(ctx, th);
				if (result == -1 ||
					result == -2) {
					assert(CAS(&ctx->inGlobal, 1, 0));
					ctxMgrReleaseContext(ctx);
					sleep = true;
					break;
				} else {
					if (workerPoolHaveWork()) { 
						contextQueuePush(M->queue, ctx); 
						break;
					}
				}
			} while(true);
		} else {
			sleep = true;
		}

	
		int num = ctxMgrGetContextNum(); 
		if (num == 0) {
			workerPoolStop(); 
			break;
		} 

		if (sleep) {
			threadSleep();
		}
	}
	return NULL;
}

int workerPoolInit(int threadNum) {
	struct workerPool *pool = (struct workerPool*)zcalloc(sizeof(*pool));
	pool->threadNum = threadNum; 
	pool->sleepThreadNum = 0;
	pool->threads = (struct thread**)zcalloc(sizeof(struct thread*)*threadNum);
	pool->running = 0;
	int i;
	for (i = 0; i < pool->threadNum; i++) {
		pool->threads[i] = threadCreate(handle, "");
	}

	pool->queue = contextQueueCreate(1024);
	pthread_mutex_init(&pool->lock, NULL);
	pthread_cond_init(&pool->cond, NULL);
	M = pool;
	return 0;
}

void workerPoolUninit() {
	assert(M != NULL);

	int i;
	for (i=0; i < M->threadNum; i++) {
		threadRelease(M->threads[i]);
	}
	zfree(M->threads);
	contextQueueRelease(M->queue);
	pthread_mutex_destroy(&M->lock);
	pthread_cond_destroy(&M->cond);
	zfree(M);
}

int workerPoolStart() {
	assert(M != NULL);
	M->running = 1;
	int i;
	for (i=0; i < M->threadNum; i++) {
		threadStart(M->threads[i]);
	}
	return 0;
}

void workerPoolJoin() {
	assert(M != NULL);
	int i;
	for (i=0; i < M->threadNum; i++) {
		threadJoin(M->threads[i]);	
	}
}

int workerPoolSend(struct context *ctx, 
					uint32_t from,
					uint32_t to,
					uint32_t session,
					uint32_t type,
					void *data,
					size_t sz,
					uint32_t ud) {

	char from_name[32];
	snprintf(from_name, 32, "c_%d", from);
	if (from == 0) {
		if (ctx) {
			from = ctx->id;
			snprintf(from_name, 32, "%s", ctx->name);
		}
	}

	if (to == 0) {
		return -1;
	}

	struct context *toctx = ctxMgrGetContext(to);
	if (toctx == NULL) {
		fprintf(stderr, "send to service(%d) type(%d),not found service!\n", to, type);
		if (data) {
			zfree(data);
		}
		return -1;
	}

	struct message msg;
	msg.from = from;

	if (type & MSG_TYPE_NEWSESSION) {
		msg.session = contextNewsession(ctx);
	} else {
		msg.session = session;
	}

	msg.type = type;
	msg.ud = ud;
	msg.sz = sz;
	msg.data = data;
	
	if (!messageQueuePush(toctx->queue, &msg)) {
		fprintf(stderr, "context(%d) queue full, size(%d)!\n", 
							toctx->id,
							messageQueueSize(toctx->queue));

		assert(0);
		ctxMgrReleaseContext(toctx);
		return -1;
	}
	
	logInfo("from(%s) to(%s) type(%X)", from_name, toctx->name);
	if (CAS(&toctx->inGlobal, 0, 1)) {
		contextQueuePush(M->queue, toctx);	
		threadWakeUp();
	} else {
		ctxMgrReleaseContext(toctx);
	}
	return msg.session;
}

void workerPoolStop() {
	pthread_mutex_lock(&M->lock);
	M->sleepThreadNum = 0;
	M->running = 0;
	pthread_cond_broadcast(&M->cond);
	pthread_mutex_unlock(&M->lock);
}


