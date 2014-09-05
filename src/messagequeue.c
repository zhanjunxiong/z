#include "messagequeue.h"

#include "atomic_ops.h"
#include "zmalloc.h"

#include <assert.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>

static uint32_t nextpow2(uint32_t num) {
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return ++num;
}


struct messageQueue *messageQueueCreate(uint32_t size) {
	struct messageQueue *queue = (struct messageQueue*)zmalloc(sizeof(*queue));
	queue->count = 0;	
	queue->writeIndex = 0;
	queue->readIndex = 0;
	queue->maximumReadIndex = 0;
	size = nextpow2(size);
	queue->messages = (struct message*)zcalloc(sizeof(struct message) * size);
	queue->size = size;
	return queue;
}

void messageQueueRelease(struct messageQueue *queue) {
	assert(queue != NULL);

	if (queue->messages) {
		zfree(queue->messages);
	}
	zfree(queue);
}

uint32_t messageQueueSize(struct messageQueue *queue) {
	return queue->count;
}

static uint32_t countToIndex(struct messageQueue *queue, uint32_t count) {
	return count & (queue->size-1);
	//return count % queue->size;
}

bool messageQueuePush(struct messageQueue *queue, struct message *msg) {
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

	queue->messages[countToIndex(queue, currentWriteIndex)] = *msg;

	while(!CAS(&queue->maximumReadIndex, currentWriteIndex, currentWriteIndex + 1))
	{
		sched_yield();
	}

	AtomicAdd(&queue->count, 1);
	return true;
}

bool messageQueuePop(struct messageQueue *queue, struct message *msg) {
	assert(msg != NULL);

	uint32_t currentMaximumReadIndex;
	uint32_t currentReadIndex;

	do
	{
		currentReadIndex = queue->readIndex;
		currentMaximumReadIndex = queue->maximumReadIndex;

		if (countToIndex(queue, currentReadIndex) ==
				countToIndex(queue, currentMaximumReadIndex))
		{
			return false;
		}

		*msg = queue->messages[countToIndex(queue, currentReadIndex)];

		if (CAS(&queue->readIndex, currentReadIndex, currentReadIndex + 1))
		{
			AtomicSub(&queue->count, 1);
			return true;
		}
	} while(1);

	assert(0);
	return false;
}


