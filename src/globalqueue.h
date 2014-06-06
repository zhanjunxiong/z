#ifndef GLOBALQUEUE_H_
#define GLOBALQUEUE_H_


struct context;
struct blockQueue;
struct blockQueue* globalqueueGet();

void globalqueuePut(struct context*);
struct context* globalqueueTake();

int globalqueueLength();

void globalqueueExit();

int globalqueueInit();
void globalqueueUninit();

#endif

