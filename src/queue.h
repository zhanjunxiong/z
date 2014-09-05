#ifndef QUEUE_H_
#define QUEUE_H_

#include "define.h"

struct queue;
struct queue* queueCreate(uint32_t maxSize, void* ud);
void queueRelease(struct queue*);

int queueForcePush(struct queue*, void*);
int queuePush(struct queue*, void*);
void* queueTake(struct queue*);

uint32_t queueLength(struct queue*);

#endif

