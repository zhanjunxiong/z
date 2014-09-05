#include "contextqueue.h"

#include "atomic_ops.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdio.h>
#include <sched.h>

static uint32_t nextpow2(uint32_t num) {
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return ++num;
}


struct contextQueue *contextQueueCreate(uint32_t size) {
	struct contextQueue *queue = (struct contextQueue*)zmalloc(sizeof(*queue));
	queue->count = 0;	
	queue->writeIndex = 0;
	queue->readIndex = 0;
	queue->maximumReadIndex = 0;
	size = nextpow2(size);
	queue->contexts = (struct context**)zcalloc(sizeof(struct context*) * size);
	queue->size = size;
	return queue;
}

void contextQueueRelease(struct contextQueue *queue) {
	assert(queue != NULL);

	if (queue->contexts) {
		zfree(queue->contexts);
	}
	zfree(queue);
}

uint32_t contextQueueSize(struct contextQueue *queue) {
	return queue->count;
}

static uint32_t countToIndex(struct contextQueue *queue, uint32_t count) {
	return count & (queue->size-1);
	//return count % queue->size;
}

bool contextQueuePush(struct contextQueue *queue, struct context *ctx) {
	uint32_t currentReadIndex;
	uint32_t currentWriteIndex;

	do
	{
		currentWriteIndex = queue->writeIndex;
		currentReadIndex = queue->readIndex;

		if (countToIndex(queue, currentWriteIndex + 1) ==
				countToIndex(queue, currentReadIndex))
		{
			return false;
		}
	} while (!CAS(&queue->writeIndex, currentWriteIndex, currentWriteIndex + 1));

	queue->contexts[countToIndex(queue, currentWriteIndex)] = ctx;

	while(!CAS(&queue->maximumReadIndex, currentWriteIndex, currentWriteIndex + 1))
	{
		sched_yield();
	}

	AtomicAdd(&queue->count, 1);
	return true;
}

struct context *contextQueuePop(struct contextQueue *queue) {

	uint32_t currentMaximumReadIndex;
	uint32_t currentReadIndex;

	struct context *ctx = NULL;
	do
	{
		currentReadIndex = queue->readIndex;
		currentMaximumReadIndex = queue->maximumReadIndex;

		if (countToIndex(queue, currentReadIndex) ==
				countToIndex(queue, currentMaximumReadIndex))
		{
			return NULL;
		}

		ctx = queue->contexts[countToIndex(queue, currentReadIndex)];

		if (CAS(&queue->readIndex, currentReadIndex, currentReadIndex + 1))
		{
			AtomicSub(&queue->count, 1);
			return ctx;
		}
	} while(1);

	assert(0);
	return NULL;
}


