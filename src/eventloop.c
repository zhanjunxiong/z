#include "eventloop.h"

#include "ae.h"
#include "thread.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdio.h>

struct eventloop{
	struct aeEventLoop *eventloop;
	struct thread *thread;
};

static struct eventloop *M = NULL;
static void* run(void *ud) {
	aeMain(M->eventloop);
	return NULL;
}

int eventloopInit(int setsize) {
	M = (struct eventloop *)zmalloc(sizeof(*M));
	M->eventloop = aeCreateEventLoop(setsize);
	M->thread = threadCreate(run, "eventloop");
	return 0;
}

void eventloopUninit() {
	threadRelease(M->thread);
	aeDeleteEventLoop(M->eventloop);
	zfree(M);
}

void eventloopStart() {
	threadStart(M->thread);
}

void eventloopStop() {
	aeStop(M->eventloop);
}

void eventloopJoin() {
	threadJoin(M->thread);
}

struct aeEventLoop* eventloopBase() {
	return M->eventloop;
}

