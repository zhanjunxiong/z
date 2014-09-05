#ifndef MESSAGEQUEUE_H_
#define MESSAGEQUEUE_H_

#include "message.h"

struct messageQueue {
	volatile uint32_t count;
	volatile uint32_t writeIndex;
	volatile uint32_t readIndex;
	volatile uint32_t maximumReadIndex;
	struct message *messages;
	uint32_t size;
};

struct messageQueue *messageQueueCreate(uint32_t size);
void messageQueueRelease(struct messageQueue *queue);

uint32_t messageQueueSize(struct messageQueue *queue);
bool messageQueuePush(struct messageQueue *queue, struct message *msg);
bool messageQueuePop(struct messageQueue *queue, struct message *msg);

#endif

