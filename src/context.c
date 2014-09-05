#include "context.h"

#include "define.h"
#include "messagequeue.h"
#include "atomic_ops.h"
#include "module.h"
#include "thread.h"
#include "z.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

struct context *contextCreate(const char *modname, 
								const char *param) {
	assert(modname != NULL);

	struct module *mod = moduleQuery(modname);
	if (mod == NULL) {
		fprintf(stderr, "not found modname(%s)\n", modname);
		return NULL;
	}

	void *inst = moduleInstanceCreate(mod);
	if (inst == NULL) {
		fprintf(stderr, "create mod(%s) fail\n", modname);
		return NULL;
	}
	
	struct context *ctx = (struct context*)zcalloc(sizeof(*ctx));
	if(ctx == NULL) {
		fprintf(stderr, "malloc context(%s) fail\n", modname);
		return NULL;
	}

	ctx->sessionid = 1;
	ctx->mod = mod;
	ctx->instance = inst;
	ctx->ref = 1;
	ctx->queue = messageQueueCreate(1024);
	return ctx;
}

struct context *contextRelease(struct context *ctx) {
	if (AtomicSub(&ctx->ref, 1) == 1) {
		if (ctx->mod) {
			moduleInstanceRelease(ctx->mod, ctx->instance);
		}
		if (ctx->queue) {
			messageQueueRelease(ctx->queue);
		}
		zfree(ctx);	
		return NULL;
	}
	return ctx;
}

void contextSetCallback(struct context *ctx, 
						contextCallback cb, 
						void *ud) {
	ctx->cb = cb;
	ctx->ud = ud;
}

uint32_t contextNewsession(struct context *ctx) {
	uint32_t id =  AtomicAdd(&ctx->sessionid, 1);
	if (CAS(&ctx->sessionid, 0, 1)) {
		return ctx->sessionid;
	}
	return id;
}

int contextDispatchMessage(struct context *ctx, struct thread *th) {
	struct message msg;
	if (!messageQueuePop(ctx->queue, &msg)) {
		return -1;
	}

	logInfo("[%ld] ctx[%s] S dispatch message type(0x%0X)", 
			threadTid(th),
			ctx->name,
			msg.type);

	int ret = -1;
	if (ctx->cb == NULL) {
		fprintf(stderr, "Drop message from (%d) to (%d) without callback, size= %ld\n", 
							msg.from, 
							ctx->id, 
							msg.sz);
	} else {
		if (msg.type == MSG_TYPE_EXIT_CONTEXT) {
			assert(contextRelease(ctx) != NULL);
			ret = -2;
		}
		else {
			ctx->cb(ctx, ctx->ud, &msg); 
			ret = 0;
		}
	}

	logInfo("[%ld] ctx[%s] E dispatch message type(0x%0X)", 
			threadTid(th),
			ctx->name,
			msg.type);
	
	if (msg.data) {
		zfree(msg.data);
	}
	return ret;
}

void contextRetain(struct context *ctx) {
	if (ctx) {
		AtomicAdd(&ctx->ref, 1);
	}
}


