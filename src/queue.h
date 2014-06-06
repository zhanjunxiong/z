#ifndef QUEUE_H_
#define QUEUE_H_

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

#define QUEUE_IN_GLOBAL 1

struct queue {
	pthread_mutex_t lock;
	struct circQueue* cq;	
	void* ud;
};

struct queue* queueCreate(uint32_t maxSize, void* ud);
void queueRelease(struct queue*);

void queuePush(struct queue*, void*);
void* queueTake(struct queue*);
size_t queueLength(struct queue*);

#endif

