#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

int timerTimeout(int to, int time, uint32_t session, int noMore);
void timerCancel(uint32_t id);

int timerStart();
void timerStop();
void timerJoin();

int timerInit();
void timerUninit();

#endif
