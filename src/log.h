#ifndef LOG_H_
#define LOG_H_


int logLog(const char* from, const char* fmt, ...);

int logInit();
void logUninit();

#endif

