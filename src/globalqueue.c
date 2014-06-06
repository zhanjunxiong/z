#include "globalqueue.h"

#include "blockqueue.h"
#include "context.h"

static struct blockQueue* BQ = NULL;
int globalqueueInit() {
	BQ = blockQueueCreate(128);
	return 0;
}

void globalqueueUninit() {
	blockQueueRelease(BQ);
}

void globalqueueExit() {
	blockQueueBroadcast(BQ);
}

struct blockQueue* globalqueueGet() {
	return BQ;
}

#include <stdio.h>
void globalqueuePut(struct context* ctx) {
	if (blockQueuePut(BQ, ctx) < 0) {
		fprintf(stderr, "put global queue fail\n");
		return;
	}
	// 引用计数加1, 防止ctx 被释放掉
	contextGrab(ctx);
}

struct context* globalqueueTake() {
	struct context* ctx = (struct context*)blockQueueTake(BQ); 
	return ctx;
}

int globalqueueLength() {
	return blockQueueLength(BQ); 
}


