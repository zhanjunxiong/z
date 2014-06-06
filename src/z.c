#include "z.h"

#include "ctx_mgr.h"

int zSend(struct context* ctx, 
			const char* from,
			const char* to,
			uint32_t session,
			uint32_t type,
			void* data, 
			size_t sz) {

	if (from == NULL) {
		from = ctx->name;
	}

	if (to == NULL) {
		return session;
	}

	if ((session = contextSend(NULL, from, to, session, type, data, sz)) < 0 ) {
		fprintf(stderr, "Drop message from (%s) to (%s) (type=%d)(size=%zu)\n", 
				from, 
				to, 
				type,
				sz);
		return -1;
	}
	
	return session;
}

const char* zLauchCmd(struct context* ctx, const char* cmd, const char* param) {
	size_t sz = strlen(param);
	char tmp[sz+1];
	strcpy(tmp, param);
	char* args = tmp;
	char* name = strsep(&args, " \t\r\n");
	char* mod = strsep(&args, " \t\r\n");
	args = strsep(&args, "\r\n");

	struct context* inst = contextCreate(name, mod, args);
	if (inst == NULL) {
		return NULL;
	} else {
		return name;
	}
}

const char* zGetEnvCmd(struct context* ctx, const char* cmd, const char* param) {
	return configGet(param, "");
}

const char* zTimeoutCmd(struct context* ctx, const char* cmd, const char* param) {

	size_t sz = strlen(param);
	char tmp[sz+1];
	strcpy(tmp, param);
	char* args = tmp;
	char* timeStr = strsep(&args, " \t\r\n");
	char* noMoreStr = strsep(&args, " \t\r\n");

	int time = strtol(param, &timeStr, 10);
	int noMore = strtol(timeStr, &noMoreStr, 10);

	int session = contextNewsession(ctx);
	int sn = timerTimeout(ctx->name, time, session, noMore);
	sprintf(ctx->result, "%d %d", session, sn);
	return ctx->result;
}


const char* zExitCmd(struct context* ctx, const char* cmd, const char* param) {
	const char* name = param;
	struct context* context = ctxMgrGetContext(name);
	contextGrab(context);
	__sync_lock_test_and_set(&context->exit, 1);
	contextRelease(context);

	ctxMgrRemoveContext(name);
	return NULL;
}

struct commandFunc {
	const char* name;
	const char* (*func)(struct context* ctx, const char* cmd, const char* parm);
};

static struct commandFunc cmdFunc[] = {
	{"LAUNCH", zLauchCmd},
	{"GETENV", zGetEnvCmd},
	{"TIMEOUT", zTimeoutCmd},
	{"EXIT", zExitCmd},
	{NULL, NULL},
};

const char* zCommand(struct context* ctx, const char* cmd, const char* param) {
	struct commandFunc* method = &cmdFunc[0];
	while(method->name) {
		if (strcmp(cmd, method->name) == 0) {
			return method->func(ctx, cmd, param);
		}
		++method;
	}
	fprintf(stderr, "cmd(%s) error\n", cmd);
	return NULL;
}

void zSetCallback(struct context* ctx, 
		contextCallback cb, 
		void* ud) {
	contextSetCallback(ctx, cb, ud);
}


