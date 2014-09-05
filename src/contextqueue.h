#ifndef CONTEXTQUEUE_H_
#define CONTEXTQUEUE_H_

#include "context.h"

struct contextQueue {
	volatile uint32_t count;
	volatile uint32_t writeIndex;
	volatile uint32_t readIndex;
	volatile uint32_t maximumReadIndex;
	struct context **contexts;
	uint32_t size;
};

struct contextQueue *contextQueueCreate(uint32_t size);
void contextQueueRelease(struct contextQueue *queue);

uint32_t contextQueueSize(struct contextQueue *queue);
bool contextQueuePush(struct contextQueue *queue, struct context *ctx);
struct context *contextQueuePop(struct contextQueue *queue);

#endif

