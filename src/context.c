#include "context.h"

#include "ctx_mgr.h"
#include "module.h"
#include "queue.h"
#include "globalqueue.h"
#include "log.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

struct context* contextCreate(const char* name, const char* modname, const char* param) {
	assert(name != NULL);

	struct module* mod = NULL;
	void* inst = NULL;
	if (modname) {
		mod = moduleQuery(modname);
		if (mod == NULL) {
			fprintf(stderr, "not found modname(%s)\n", modname);
			return NULL;
		}

		inst = moduleInstanceCreate(mod);
		if (inst == NULL) {
			fprintf(stderr, "create mod(%s) fail\n", modname);
			return NULL;
		}
	}
	
	struct context* ctx = (struct context*)zcalloc(sizeof(*ctx));
	if(ctx == NULL) {
		fprintf(stderr, "malloc context fail\n");
		return NULL;
	}

	ctx->init = false;
	ctx->mod = mod;
	ctx->instance = inst;


	size_t len = strlen(name) + 1;			
	size_t maxlen = len > CONTEXT_NAME_MAX_LEN-1 ? CONTEXT_NAME_MAX_LEN-1 : len;
	memcpy(ctx->name, name, maxlen);

	ctx->ref = 1;
	ctx->queue = queueCreate(128, ctx);
	// 注册 ctx 到管理器中
	if (ctxMgrAddContext(ctx->name, ctx) < 0) {
		fprintf(stderr, "name %s is exist\n", name);
		return NULL;
	}
	//__sync_synchronize();
	// ctx 初始化完成	
	ctx->init = true;
	if (mod) {
		int r = moduleInstanceInit(mod, inst, ctx, param);
		if (r != 0 ) {
			fprintf(stderr, "launch %s, modname(%s) fail\n", name, modname);

			ctxMgrRemoveContext(name);
			return NULL;
		}
	}

	//fprintf(stderr, "create context name(%s)\n", ctx->name);
	return ctx;
}

static void _deleteContext(struct context* ctx) {
	//fprintf(stderr, "delete context name(%s)\n", ctx->name);
	if (ctx->mod) {
		moduleInstanceRelease(ctx->mod, ctx->instance);
	}
	if (ctx->queue) {
		queueRelease(ctx->queue);
	}
	zfree(ctx);
}

struct context* contextRelease(struct context* ctx) {
	if (__sync_sub_and_fetch(&ctx->ref,1) == 0) {
		_deleteContext(ctx);
		return NULL;
	}
	return ctx;
}

uint32_t contextNewsession(struct context* ctx) {
	// session always be a positive number
	uint32_t id = (++ctx->sessionid) & 0x7fffffff;
	return id;
}

void contextGrab(struct context* ctx) {
	__sync_add_and_fetch(&ctx->ref,1);
}

int _dispatchMessage(struct context* ctx, struct message *msg) {
	assert(ctx->init);

	if (ctx->cb == NULL) {
		fprintf(stderr, "Drop message from (%s) to (%s) without callback, size= %d", 
							msg->from, 
							ctx->name, 
							msg->sz);
		messageRelease(msg);	
		return -1;
	}

	if (!ctx->cb(ctx, ctx->ud, msg)) {
		messageRelease(msg);
	}
	return 0;
}

int contextDispatchMessage(struct context* ctx) {
	struct message* msg = (struct message*)queueTake(ctx->queue);
	if (msg == NULL) {
		return -1;
	}
	return _dispatchMessage(ctx, msg); 
}

void contextSetCallback(struct context* ctx, contextCallback cb, void* ud) {
	ctx->cb = cb;
	ctx->ud = ud;
}

int contextSend(struct context* ctx, 
				const char* from,
				const char* to,
				uint32_t session,
				uint32_t type,
				void* data, 
				size_t sz) {

	assert(!(ctx == NULL && to == NULL));
	if (ctx == NULL) {
		ctx = ctxMgrGetContext(to);
		if (ctx == NULL) {
			fprintf(stderr, "send to service(%s) type(%d), not found service!\n", to, type);
		}
		//assert(ctx != NULL);
	}

	if (ctx == NULL) {
		fprintf(stderr, "contextSend, ctx is null, ctx name: %s\n", to);
		return -1;
	}

	contextGrab(ctx); 
	int ret = -1;
	struct message* msg = messageCreate(ctx, from, session, type, data, sz);
	if (msg) {
		queuePush(ctx->queue, msg);  
		ret = msg->session;
		// 有消息要处理了 把ctx 放到调度队列中
		if(__sync_lock_test_and_set(&ctx->state, 1) == 0) {	
			globalqueuePut(ctx);
		}
	}
	contextRelease(ctx);
	return ret;
}

