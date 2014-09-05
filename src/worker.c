#include "worker.h"

#include "contextqueue.h"
#include "atomic_ops.h"
#include "ctx_mgr.h"
#include "thread.h"
#include "worker_pool.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdio.h>

static void *handle(void *ud) {
	struct worker *worker = (struct worker*)ud;

	while (true) {
		struct context *ctx = workerPoolPopContext(); 
		if (ctx == NULL) {
			if (ctxMgrGetContextNum() <= 0) {
				break;
			} 

			workerSleep(worker);
		} else {

			do {
				int result = contextDispatchMessage(ctx);
				if (result == -1) {
					CAS(&ctx->inGlobal, 1, 0);
					contextRelease(ctx);
					break;
				} else {
					if (!workerPoolHaveWork()) { 
						workerPoolPushContext(ctx); 
						break;
					}
				}
			} while(true);

		}
	}
	return NULL;
}

struct worker *workerCreate() {
	struct worker *worker = (struct worker*)zmalloc(sizeof(*worker));
	worker->queue = contextQueueCreate(1024);
	worker->state = WORK_STATE;

	worker->thread = threadCreate(handle, "");
	threadSetud(worker->thread, worker);

	pthread_mutex_init(&worker->lock, NULL);
	pthread_cond_init(&worker->cond, NULL);
	
	static int id = 1;
	worker->id = id++;
	return worker;
}

void workerRelease(struct worker* worker) {
	contextQueueRelease(worker->queue);
	threadRelease(worker->thread);
	pthread_cond_destroy(&worker->cond);
	pthread_mutex_destroy(&worker->lock);
	zfree(worker);
}

int workerStart(struct worker* worker) {
	threadStart(worker->thread);
	return 0;
}

void workerJoin(struct worker *worker) {
	threadJoin(worker->thread);
}

void workerSleep(struct worker *worker) {
	if (CAS(&worker->state, WORK_STATE, IDLE_STATE)) {
		pthread_mutex_lock(&worker->lock);
		pthread_cond_wait(&worker->cond, &worker->lock);
		pthread_mutex_unlock(&worker->lock);

		workerPoolAddWorker(); 
		return;
	}

	assert(0);
}

void workerWakeUp(struct worker *worker) {
	pthread_mutex_lock(&worker->lock);
	pthread_cond_signal(&worker->cond);
	pthread_mutex_unlock(&worker->lock);

	workerPoolSubWorker(); 
}

