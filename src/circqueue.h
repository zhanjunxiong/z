#ifndef CIRC_QUEUE_H_
#define CIRC_QUEUE_H_

#include "define.h"

struct circQueue;

struct circQueue* circQueueCreate(uint32_t size);
void circQueueRelease(struct circQueue*);

int circQueueForcePushTail(struct circQueue*, void*);
int circQueuePushTail(struct circQueue*, void*);
void* circQueuePopHead(struct circQueue*);

uint32_t circQueueLength(struct circQueue*);

bool circQueueIsEmpty(struct circQueue*);
bool circQueueIsFull(struct circQueue*);

void circQueueClear(struct circQueue *cq); 

#endif

