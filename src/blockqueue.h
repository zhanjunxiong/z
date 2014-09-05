#ifndef BLOCK_QUEUE_H_
#define BLOCK_QUEUE_H_

#include "define.h"

struct blockQueue;

struct blockQueue* blockQueueCreate(uint32_t maxSize);
void blockQueueRelease(struct blockQueue*);

int blockQueuePush(struct blockQueue*, void*);
void* blockQueueTake(struct blockQueue*);

uint32_t blockQueueLength(struct blockQueue*);

void blockQueueExit(struct blockQueue* queue); 

#endif

