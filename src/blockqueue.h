#ifndef BLOCK_QUEUE_H_
#define BLOCK_QUEUE_H_

#include <stdint.h>
#include <stdlib.h>

struct blockQueue;

struct blockQueue* blockQueueCreate(uint32_t maxSize);
void blockQueueRelease(struct blockQueue*);

int blockQueuePut(struct blockQueue*, void*);
void* blockQueueTake(struct blockQueue*);
size_t blockQueueLength(struct blockQueue*);
void blockQueueBroadcast(struct blockQueue* queue); 

#endif

