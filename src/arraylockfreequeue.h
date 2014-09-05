#ifndef ARRAYLOCKFREEQUEUE_H_
#define ARRAYLOCKFREEQUEUE_H_

#include <stdbool.h>
#include <stdint.h>

struct arrayLockFreeQueue {
	volatile uint32_t count;
	volatile uint32_t writeIndex;
	volatile uint32_t readIndex;
	volatile uint32_t maximumReadIndex;
	void **items;
	uint32_t size;
};

struct arrayLockFreeQueue *arrayLockFreeQueueCreate(uint32_t size);
void arrayLockFreeQueueRelease(struct arrayLockFreeQueue *queue);

uint32_t arrayLockFreeQueueSize(struct arrayLockFreeQueue *queue);
bool arrayLockFreeQueuePush(struct arrayLockFreeQueue *queue, void *item);
void *arrayLockFreeQueuePop(struct arrayLockFreeQueue *queue);

#endif

