#ifndef LOG_H_
#define LOG_H_

void logInfo(const char *fmt, ...);
void logVperror(const char *fmt, ...);

void logExit();

int logInit(const char *programName, bool do_daemonize);
void logUninit();

#endif

