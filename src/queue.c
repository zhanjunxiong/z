#include "queue.h"

#include "globalqueue.h"
#include "circqueue.h"
#include "zmalloc.h"

#include <assert.h>

struct queue* queueCreate(uint32_t maxSize, void* ud) {
	struct queue* queue;
	struct circQueue* cq;
	if (!(cq = circQueueCreate(maxSize))) 
		return NULL;

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
	circQueueRelease(queue->cq);
	pthread_mutex_destroy(&queue->lock);
	queue->ud = NULL;
	zfree(queue);
}

#include <stdio.h>
void queuePush(struct queue* queue, void* elem) {
	pthread_mutex_lock(&queue->lock);	
	int ret = circQueuePushTail(queue->cq, elem);
	if (ret < 0 ) {
		fprintf(stderr, "queuePush is full\n");
	}
	pthread_mutex_unlock(&queue->lock);
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

size_t queueLength(struct queue* queue) {
	int len = 0;
	pthread_mutex_lock(&queue->lock);
	len = circQueueLength(queue->cq);
	pthread_mutex_unlock(&queue->lock);
	return len;
}


