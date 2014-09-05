#include "log.h" 

#include "define.h"
#include "blockqueue.h"
#include "env.h"
#include "thread.h"
#include "zmalloc.h"

#include <sys/time.h>
#include <time.h>

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

enum eLogState {
	E_LOG_NULL = 0,
	E_LOG_FILE = 1,
	E_LOG_STDOUT = 2,
};

struct log {
	FILE* handle;
	char name[FILENAME_MAX];
	char rotateDir[FILENAME_MAX];
	int maxLineNum;
	int lineNum;
	// 0 不记录日志
	// 1 把日志记录到文件
	// 2 把日志记录打印到终端
	int state;
	struct blockQueue* queue;
	struct thread* thread;
	int exit;
};

static struct log* LOG = NULL;
void logInfo(const char *fmt, ...) {

	if (LOG->exit == 0) {
		return;
	}

	if (LOG->state == E_LOG_NULL) {
		// 不记录日志
		return;
	}

 	char* buf = (char*)zmalloc(1024);
	int bufLen = 0;
	va_list ap;
	va_start(ap, fmt);
	bufLen = vsnprintf(buf, 1024, fmt, ap); 
	if (bufLen == -1) {
		buf[1023] = '\0';
	}
	va_end(ap);

	if (blockQueuePush(LOG->queue, buf) < 0) {
		fprintf(stderr, "log thread queue is full, buf(%s)\n", buf);
		zfree(buf);
		return;
	}
}

void logVperror(const char *fmt, ...) {
	int old_errno = errno;
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	if (vsnprintf(buf, sizeof(buf), fmt, ap) == -1) {
		buf[sizeof(buf) - 1] = '\0';
	}
	va_end(ap);

	errno = old_errno;
	perror(buf);
}

static void _rotateFile(struct tm* tm) {
	if (LOG->state == E_LOG_FILE) { 
		if (LOG->lineNum++ > LOG->maxLineNum) {
			fclose(LOG->handle);

			char filePath[FILENAME_MAX];
			memset(filePath, 0, FILENAME_MAX);
			snprintf(filePath, FILENAME_MAX - 1, "%s/%s_%04d_%02d_%02d_%02d_%02d_%02d",
								LOG->rotateDir,
								LOG->name,
								tm->tm_year + 1900,
								tm->tm_mon + 1,
								tm->tm_mday,
								tm->tm_hour,
								tm->tm_min,
								tm->tm_sec);
			

			rename(LOG->name, filePath);
			
			LOG->lineNum = 0; 
			LOG->handle = fopen(LOG->name,"w");
			if (LOG->handle == NULL) {
				fprintf(stderr, "rotate file faile, open filename(%s) fail\n", LOG->name);
				return;
			}
		}
	}
}


static int handleLog(char* buf) {
	assert(buf != NULL);

	struct timeval t;
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	_rotateFile(tm); 

	fprintf(LOG->handle, "[%04d-%02d-%02d %02d:%02d:%02d:%06ld][sys]",
						tm->tm_year+1900,
						tm->tm_mon+1,
						tm->tm_mday,
						tm->tm_hour,
						tm->tm_min,
						tm->tm_sec,
						t.tv_usec);

	fprintf(LOG->handle, buf, strlen(buf));
	fprintf(LOG->handle, "\n");
	fflush(LOG->handle);

	return 0;
}

static void* log(void* ud) {
	while(true) {
		char* buf = (char*)blockQueueTake(LOG->queue);
		if (buf == NULL) {
			if (blockQueueLength(LOG->queue) == 0) {
				break;
			} else {
				logExit();
			}
		} else {
			handleLog(buf);	
			zfree(buf);
		}
	}

	LOG->exit = 0;
	return NULL;
}

void logExit() {
	if (LOG->queue) {
		blockQueueExit(LOG->queue); 
	}
}

int logInit(const char *programName, bool do_daemonize) {
	assert(programName != NULL);

	LOG = (struct log*)zmalloc(sizeof(*LOG));

	int state = envGetInt("z_log_state", 0);
	if (strcmp(programName, Z_PROGNAME) == 0) {
		state = E_LOG_NULL; 
	}

	/*
	if (!do_daemonize && state == E_LOG_FILE) {
		state = E_LOG_STDOUT;
	}
	*/

	if (state == E_LOG_FILE) {
		char defaultFile[FILENAME_MAX];
		snprintf(defaultFile, FILENAME_MAX, "%s_sys.log", programName);
		const char* name = envGet("z_log_name", defaultFile);
		int maxLineNum = envGetInt("z_log_maxlinenum", 200000);
		const char* rotateDir = envGet("z_log_rotatedir", "");
		FILE* handle = fopen(name, "w+");
		if (handle == NULL) {
			return -1;
		}
		
		LOG->handle = handle;
		snprintf(LOG->name, FILENAME_MAX, "%s", name);
		snprintf(LOG->rotateDir, FILENAME_MAX, "%s", rotateDir);
		LOG->maxLineNum = maxLineNum;

	} else if (state == E_LOG_STDOUT) {
		LOG->handle = stdout;
	}

	LOG->exit = 1;
	LOG->lineNum = 0;
	LOG->queue = blockQueueCreate(SYS_LOG_QUEUE_LEN);
	LOG->state = state;

	if (state != E_LOG_NULL) {
		LOG->thread = threadCreate(log, "log");
		threadStart(LOG->thread);
	}

	return 0;
}

void logUninit() {
	
	if (LOG->state != E_LOG_NULL) {
		threadJoin(LOG->thread);
		threadRelease(LOG->thread);
	}

	if (LOG->state == E_LOG_FILE) {
		fclose(LOG->handle);
	}
	if (LOG->queue) {
		blockQueueRelease(LOG->queue);
	}
	zfree(LOG);
}


