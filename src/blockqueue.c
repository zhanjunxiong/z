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

int blockQueuePush(struct blockQueue* queue, void* elem) {
	pthread_mutex_lock(&queue->lock);
	if (circQueuePushTail(queue->cq, elem)<0) {
		pthread_mutex_unlock(&queue->lock);
		fprintf(stderr, "please change queue length, or some error");
		// todo exit??
		// os.exit(1);
		return -1;
	}
	pthread_cond_signal(&queue->cond);
	pthread_mutex_unlock(&queue->lock);
	return 0;
}

void blockQueueExit(struct blockQueue* queue) {
	pthread_mutex_lock(&queue->lock);
	if (circQueueForcePushTail(queue->cq, NULL) < 0) {
		fprintf(stderr, "blockQueueExit fail, circQueueForcePushTail fail\n");
	} 
	pthread_cond_signal(&queue->cond);
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

uint32_t blockQueueLength(struct blockQueue* queue) {
	uint32_t len = 0;
	pthread_mutex_lock(&queue->lock);
	len = circQueueLength(queue->cq);
	pthread_mutex_unlock(&queue->lock);
	return len;
}

