#include "eventloop.h"

#include "ae.h"

#include <assert.h>

static struct aeEventLoop* BASE = NULL;
int eventloopInit() {
	BASE = aeCreateEventLoop();
	return 0;
}

void eventloopUninit() {
	aeDeleteEventLoop(BASE);
}

void eventloopStart() {
	assert(BASE != NULL);
	aeMain(BASE);
}

void eventloopStop() {
	aeStop(BASE);
}

struct aeEventLoop* eventloopBase() {
	return BASE;
}

