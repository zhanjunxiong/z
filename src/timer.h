#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

int timerTimeout(const char* to, int time, uint32_t session, int noMore);
void timerCancel(uint32_t id);

void timerStop();
void timerMain();

int timerInit();
void timerUninit();

#endif
