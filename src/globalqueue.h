#ifndef GLOBALQUEUE_H_
#define GLOBALQUEUE_H_

struct context;

int globalqueuePush(struct context*);
struct context* globalqueueTake();

int globalqueueLength();

void globalqueueExit();

int globalqueueInit();
void globalqueueUninit();

#endif

