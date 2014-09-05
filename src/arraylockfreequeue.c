#include "arraylockfreequeue.h"

#include "atomic_ops.h"
#include "zmalloc.h"

#include <assert.h>
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



struct arrayLockFreeQueue *arrayLockFreeQueueCreate(uint32_t size) {
	struct arrayLockFreeQueue *queue = (struct arrayLockFreeQueue*)zmalloc(sizeof(*queue));
	queue->count = 0;	
	queue->writeIndex = 0;
	queue->readIndex = 0;
	queue->maximumReadIndex = 0;
	size = nextpow2(size);
	queue->items = (void**)zcalloc(sizeof(void*) * size);
	queue->size = size;
	return queue;
}

void arrayLockFreeQueueRelease(struct arrayLockFreeQueue *queue) {
	assert(queue != NULL);

	if (queue->items) {
		zfree(queue->items);
	}
	zfree(queue);
}

uint32_t arrayLockFreeQueueSize(struct arrayLockFreeQueue *queue) {
	return queue->count;
}

static uint32_t countToIndex(struct arrayLockFreeQueue *queue, uint32_t count) {
	return count & (queue->size-1);
	//return count % queue->size;
}

bool arrayLockFreeQueuePush(struct arrayLockFreeQueue *queue, void *item) {
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

	queue->items[countToIndex(queue, currentWriteIndex)] = item;

	while(!CAS(&queue->maximumReadIndex, currentWriteIndex, currentWriteIndex + 1))
	{
		sched_yield();
	}

	AtomicAdd(&queue->count, 1);
	return true;
}

void *arrayLockFreeQueuePop(struct arrayLockFreeQueue *queue) {

	uint32_t currentMaximumReadIndex;
	uint32_t currentReadIndex;

	void *item = NULL;
	do
	{
		currentReadIndex = queue->readIndex;
		currentMaximumReadIndex = queue->maximumReadIndex;

		if (countToIndex(queue, currentReadIndex) ==
				countToIndex(queue, currentMaximumReadIndex))
		{
			return NULL;
		}

		item = queue->items[countToIndex(queue, currentReadIndex)];

		if (CAS(&queue->readIndex, currentReadIndex, currentReadIndex + 1))
		{
			AtomicSub(&queue->count, 1);
			return item;
		}
	} while(1);

	assert(0);
	return NULL;
}


