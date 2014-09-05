#include "z.h"

#include "worker_pool.h"
#include "context.h"

int zSend(struct context *ctx, 
			int from,
			int to,
			uint32_t session,
			uint32_t type,
			void *data, 
			size_t sz)  {

	int ret =  workerPoolSend(ctx, 
							from,
							to,
							session,
							type,
							data,
							sz,
							0);
	return ret;
}

int zSendx(struct context *ctx, 
			uint32_t from,
			uint32_t to,
			uint32_t session,
			uint32_t type,
			void *data, 
			size_t sz,
			uint32_t ud) {

	return workerPoolSend(ctx, 
							from,
							to,
							session,
							type,
							data,
							sz,
							ud);
}



const char* zLauchCmd(struct context* ctx, const char* cmd, const char* param) {
	size_t sz = strlen(param);
	char tmp[sz+1];
	memset(tmp, 0, sz+1);
	strcpy(tmp, param);

	char* args = tmp;
	char* mod = strsep(&args, " \t\r\n");
	args = strsep(&args, "\r\n");
	//fprintf(stderr, "load mod(%s) args(%s)\n", mod, args);
	struct context *inst = ctxMgrCreateContext(mod, args);
	if (inst == NULL) {
		return NULL;
	} else {
		snprintf(ctx->result, 32, "%d", inst->id);
		return ctx->result;
	}
}

const char* zGetEnvCmd(struct context* ctx, const char* cmd, const char* param) {
	return envGet(param, "");
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
	int sn = timerTimeout(ctx->id, time, session, noMore);
	snprintf(ctx->result, 32, "%d %d", session, sn);
	return ctx->result;
}

const char* zTimerCancel(struct context* ctx, const char* cmd, const char* param) {

	size_t sz = strlen(param);
	char tmp[sz+1];
	strcpy(tmp, param);
	char* args = tmp;

	char* idStr = strsep(&args, " \t\r\n");
	uint32_t id = strtol(param, &idStr, 10);

	timerCancel(id);
	return NULL;
}

const char* zExitCmd(struct context* ctx, const char* cmd, const char* param) {
	size_t sz = strlen(param);
	char tmp[sz+1];
	strcpy(tmp, param);
	char* args = tmp;

	char* toStr = strsep(&args, " \t\r\n");
	int to = strtol(param, &toStr, 10);

	workerPoolSend(ctx, 
					0,
					to,
					0,
					MSG_TYPE_EXIT_CONTEXT,
					NULL,
					0,
					0);
	return NULL;
}

const char* zQueueLenCmd(struct context* ctx, const char* cmd, const char* param) {
	
	size_t sz = strlen(param);
	char tmp[sz+1];
	strcpy(tmp, param);
	char* args = tmp;

	char* toStr = strsep(&args, " \t\r\n");
	int to = strtol(param, &toStr, 10);

	struct context *toctx = ctxMgrGetContext(to);
	if (ctx) {
		uint32_t len = messageQueueSize(toctx->queue);
		snprintf(toctx->result, 32, "%d", len);

		ctxMgrReleaseContext(toctx);
		return toctx->result;
	}
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
	{"TIMECANCEL", zTimerCancel},
	{"EXIT", zExitCmd},
	{"QLEN", zQueueLenCmd},
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

void zSetCallback(struct context *ctx, 
		contextCallback cb, 
		void *ud) {
	contextSetCallback(ctx, cb, ud);
}


