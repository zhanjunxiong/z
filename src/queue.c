#include "queue.h"

#include "circqueue.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdio.h>
#include <pthread.h>

struct queue {
	pthread_mutex_t lock;
	struct circQueue* cq;	
	void* ud;
};

struct queue* queueCreate(uint32_t maxSize, void* ud) {
	struct queue* queue;
	struct circQueue* cq;
	if (!(cq = circQueueCreate(maxSize))) {
		return NULL;
	}

	if (!(queue = (struct queue*)zmalloc(sizeof(*queue)))) {
		circQueueRelease(cq);
		return NULL;
	}
	queue->cq = cq;	
	queue->ud = ud;
	pthread_mutex_init(&queue->lock, NULL);
	return queue;
}

void queueRelease(struct queue* queue) {
	assert(queue != NULL);
	queue->ud = NULL;
	circQueueRelease(queue->cq);
	pthread_mutex_destroy(&queue->lock);
	zfree(queue);
}

int queueForcePush(struct queue* queue, void* elem) {
	int ret;
	pthread_mutex_lock(&queue->lock);	
	ret = circQueueForcePushTail(queue->cq, elem);
	if (ret < 0 ) {
		fprintf(stderr, "queueForcePush fail\n");
	}
	pthread_mutex_unlock(&queue->lock);
	return ret;
}

int queuePush(struct queue* queue, void* elem) {
	int ret;
	pthread_mutex_lock(&queue->lock);	
	ret = circQueuePushTail(queue->cq, elem);
	if (ret < 0 ) {
		fprintf(stderr, "queuePush fail, ret: %d\n", ret);
	}
	pthread_mutex_unlock(&queue->lock);
	return ret;
}

void* queueTake(struct queue* queue) {
	void* elem = NULL;
	pthread_mutex_lock(&queue->lock);
	if (!circQueueIsEmpty(queue->cq)){
		elem = circQueuePopHead(queue->cq);
	}
	pthread_mutex_unlock(&queue->lock);
	return elem;
}

uint32_t queueLength(struct queue* queue) {
	uint32_t len = 0;
	pthread_mutex_lock(&queue->lock);
	len = circQueueLength(queue->cq);
	pthread_mutex_unlock(&queue->lock);
	return len;
}


