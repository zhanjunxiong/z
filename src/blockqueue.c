#include "blockqueue.h"

#include "circqueue.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdio.h>
#include <pthread.h>

struct blockQueue {
	pthread_mutex_t lock;
	pthread_cond_t cond;
	struct circQueue* cq;	
};

struct blockQueue* blockQueueCreate(uint32_t maxSize) {
	struct blockQueue* queue;
	struct circQueue* cq;
	if (!(cq = circQueueCreate(maxSize))) 
		return NULL;

	if (!(queue = (struct blockQueue*)zmalloc(sizeof(*queue)))) {
		circQueueRelease(cq);
		return NULL;
	}
	queue->cq = cq;	
	pthread_mutex_init(&queue->lock, NULL);
	pthread_cond_init(&queue->cond, NULL);
	return queue;
}

void blockQueueRelease(struct blockQueue* queue) {
	assert(queue != NULL);
	circQueueRelease(queue->cq);
	pthread_cond_destroy(&queue->cond);
	pthread_mutex_destroy(&queue->lock);

	zfree(queue);
}

int blockQueuePut(struct blockQueue* queue, void* elem) {
	pthread_mutex_lock(&queue->lock);
	if (circQueuePushTail(queue->cq, elem)<0) {
		pthread_mutex_unlock(&queue->lock);
		return -1;
	}
	pthread_cond_signal(&queue->cond);
	pthread_mutex_unlock(&queue->lock);
	return 0;
}

#include "context.h"
void blockQueueBroadcast(struct blockQueue* queue) {
	static struct context ctx;
	ctx.state = 2;

	pthread_mutex_lock(&queue->lock);
	if (circQueuePushTail(queue->cq, (void*)&ctx) < 0) {
		fprintf(stderr, "blockQueuBroadcase fail\n");
	}
	pthread_cond_broadcast(&queue->cond);
	pthread_mutex_unlock(&queue->lock);
}


void* blockQueueTake(struct blockQueue* queue) {
	pthread_mutex_lock(&queue->lock);
	while (circQueueIsEmpty(queue->cq)) {
		pthread_cond_wait(&queue->cond, &queue->lock);
	}
	void* elem = circQueuePopHead(queue->cq);
	pthread_mutex_unlock(&queue->lock);
	return elem;
}

size_t blockQueueLength(struct blockQueue* queue) {
	int len = 0;
	pthread_mutex_lock(&queue->lock);
	len = circQueueLength(queue->cq);
	pthread_mutex_unlock(&queue->lock);
	return len;

}


