#ifndef Z_H_
#define Z_H_

#include "ctx_mgr.h"
#include "define.h"
#include "env.h"
#include "message.h"
#include "name_mgr.h"
#include "session.h"
#include "gen_tcp.h"
#include "log.h"
#include "timer.h"
#include "tcp.h"
#include "zmalloc.h"

#include <string.h>
#include <stdio.h>

const char *zCommand(struct context *ctx, 
					const char *cmd, 
					const char *param); 

int zSend(struct context *ctx, 
			int from,
			int to,
			uint32_t session,
			uint32_t type,
			void *data, 
			size_t sz); 

int zSendx(struct context *ctx, 
			uint32_t from,
			uint32_t to,
			uint32_t session,
			uint32_t type,
			void *data, 
			size_t sz,
			uint32_t ud); 


void zSetCallback(struct context *ctx, 
		contextCallback cb, 
		void *ud); 

#endif

