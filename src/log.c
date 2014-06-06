#include "log.h"

#include "config.h"
#include "z.h"
#include "zmalloc.h"

#include <stdarg.h>
#include <string.h>

static char* sLogSrvName = NULL;
static int logPrint(const char* from, int level, const char* fmt, va_list ap) {
	char text[1024];
	int len = vsnprintf(text, 1023, fmt, ap);
	zSend(NULL, from, sLogSrvName, 0, level | MSG_TYPE_COPYDATA, text, len);	
	return 0;
}

int logLog(const char* from, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logPrint(from, MSG_TYPE_LOG_LOG, fmt, args);
	va_end(args);
	return 1;
}

int logWarn(const char* from, const char* fmt, ...) {
	va_list args;
	va_start(args, fmt);
	logPrint(from, MSG_TYPE_LOG_WARN, fmt, args);
	va_end(args);
	return 1;
}

int logInit() {
	const char* name = configGet("log_srv_name",  "LOG_SRV");
	sLogSrvName = (char*)zcalloc(strlen(name) + 1);
	memcpy(sLogSrvName, name, strlen(name));
	return 0;
}

void logUninit() {
	if (sLogSrvName) {
		zfree(sLogSrvName);
		sLogSrvName = NULL;
	}
}



