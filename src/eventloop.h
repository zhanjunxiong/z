#ifndef EVENTLOOP_H_
#define EVENTLOOP_H_

int eventloopInit(int setsize);
void eventloopUninit();

void eventloopStart();
void eventloopJoin();
void eventloopStop();

struct aeEventLoop* eventloopBase();

#endif


