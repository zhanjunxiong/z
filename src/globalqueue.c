#include "globalqueue.h"

#include "blockqueue.h"

#include <stdio.h>

static struct blockQueue* BQ = NULL;

int globalqueuePush(struct context* ctx) {
	if (blockQueuePush(BQ, ctx) < 0) {
		fprintf(stderr, "globalqueuePush fail\n");
		return -1;
	}
	return 0;
}

struct context* globalqueueTake() {
	struct context* ctx = (struct context*)blockQueueTake(BQ); 
	return ctx;
}

int globalqueueLength() {
	return blockQueueLength(BQ); 
}

void globalqueueExit() {
	blockQueueExit(BQ); 
}


int globalqueueInit() {
	BQ = blockQueueCreate(1024);
	return 0;
}

void globalqueueUninit() {
	blockQueueRelease(BQ);
}


