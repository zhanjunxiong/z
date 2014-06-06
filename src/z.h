#ifndef Z_H_
#define Z_H_

#include "context.h"
#include "config.h"
#include "timer.h"
#include "zmalloc.h"

#include <string.h>
#include <stdio.h>

// 日志等级类型
#define MSG_TYPE_LOG_ERROR		0
#define MSG_TYPE_LOG_WARN		1
#define MSG_TYPE_LOG_LOG		2
#define MSG_TYPE_LOG_MSG		3
#define MSG_TYPE_LOG_DEBUG		4
#define MSG_TYPE_LOG_STAT		5
#define MSG_TYPE_LOG_LEN		6


const char* zCommand(struct context* ctx, const char* cmd, const char* param); 

int zSend(struct context* ctx, 
			const char* from,
			const char* to,
			uint32_t session,
			uint32_t type,
			void* data, 
			size_t sz); 

void zSetCallback(struct context* ctx, 
		contextCallback cb, 
		void* ud); 


#endif

