#ifndef CIRC_QUEUE_H_
#define CIRC_QUEUE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

struct circQueue;

struct circQueue* circQueueCreate(uint32_t size);
void circQueueRelease(struct circQueue*);
int circQueuePushTail(struct circQueue*, void*);
void* circQueuePopHead(struct circQueue*);
uint32_t circQueueLength(struct circQueue*);
bool circQueueIsEmpty(struct circQueue*);
bool circQueueIsFull(struct circQueue*);

#endif

