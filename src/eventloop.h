#ifndef EVENTLOOP_H_
#define EVENTLOOP_H_

int eventloopInit();
void eventloopUninit();

void eventloopStart();
void eventloopStop();

struct aeEventLoop* eventloopBase();

#endif


