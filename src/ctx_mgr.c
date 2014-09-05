#include "ctx_mgr.h"

#include "module.h"
#include "z.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

struct ctxMgr {
	pthread_rwlock_t rwlock;

	uint32_t allocId;
	uint32_t contextNum;
	struct context **contexts;
	uint32_t setsize;
};


static struct ctxMgr *ctxMgrCreate(int setsize) {
	struct ctxMgr *mgr = (struct ctxMgr*)zmalloc(sizeof(*mgr));
	pthread_rwlock_init(&mgr->rwlock, NULL);

	mgr->allocId = 0;
	mgr->contextNum = 0;
	mgr->setsize = setsize;
	mgr->contexts = (struct context**)zcalloc(sizeof(struct context*)*setsize);
	return mgr;
}

static void ctxMgrRelease(struct ctxMgr *mgr) {
	assert(mgr != NULL);

	pthread_rwlock_destroy(&mgr->rwlock);
	zfree(mgr->contexts);
	zfree(mgr);
}

static int ctxMgrAddContext(struct ctxMgr *mgr, struct context *ctx) {

	pthread_rwlock_wrlock(&mgr->rwlock);
	if (mgr->contextNum >= mgr->setsize) {
		int newsetsize = mgr->setsize * 2;
		struct context **tmp = (struct context**)
							zcalloc(sizeof(struct context*) * newsetsize);
		if (tmp == NULL) {
			pthread_rwlock_unlock(&mgr->rwlock);
			return -1;
		}

		uint32_t i;
		for (i = 0; i < mgr->setsize; i++) {
			struct context *c = mgr->contexts[i];
			if (c) {
				tmp[c->id % newsetsize] = c;	
			}
		}

		mgr->setsize = newsetsize;

		zfree(mgr->contexts);
		mgr->contexts = tmp;
	}

	uint32_t i;
	for (i=0; i < mgr->setsize; i++) {
		uint32_t id = mgr->allocId++;
		if(id <= 0) {
			mgr->allocId = 1;
			id = 1;
		}

		ctx->id = id;
		if (mgr->contexts[id % mgr->setsize] == NULL) { 
			mgr->contexts[id % mgr->setsize] = ctx; 
			mgr->contextNum++;
			pthread_rwlock_unlock(&mgr->rwlock);
			return id;
		}
	}

	pthread_rwlock_unlock(&mgr->rwlock);
	return -1;
}

static int ctxMgrRemoveContext(struct ctxMgr *mgr, int id) {
	if (id < 0) return -1;

	pthread_rwlock_wrlock(&mgr->rwlock);
	if (mgr->contexts[id % mgr->setsize]) {
		mgr->contexts[id % mgr->setsize] = NULL;
		mgr->contextNum--;
	}
	pthread_rwlock_unlock(&mgr->rwlock);
	return 0;
}

static struct ctxMgr* M = NULL;
struct context *ctxMgrCreateContext(const char* modname, 
									const char* parm) {	

	struct context *ctx = contextCreate(modname, parm);
	if (ctx == NULL) {
		fprintf(stderr, "create context modname(%s) parm(%s) fail\n", modname, parm);
		return NULL;
	}

	if (ctx->mod == NULL) {
		fprintf(stderr, "context(%s) mod is nil\n", modname);
		return NULL;
	} 

	int id = ctxMgrAddContext(M, ctx);
	if (id < 0) {
		fprintf(stderr, "can't add context\n");
		// todo don't free ctx
		return NULL;
	}

	snprintf(ctx->name, 32, "c_%d", id);

	int r = moduleInstanceInit(ctx->mod, ctx->instance, ctx, parm);
	if (r != 0 ) {
		fprintf(stderr, "launch modname(%s) fail\n", modname);
		ctxMgrRemoveContext(M, id);
		return NULL;
	}

	return ctx;
}

void ctxMgrReleaseContext(struct context *ctx) {
	assert(ctx != NULL);

	int id = ctx->id;
	if (contextRelease(ctx) == NULL) { 
		ctxMgrRemoveContext(M, id);
	}
}

struct context *ctxMgrGetContext(uint32_t id) {
	struct context *ctx = NULL;

	pthread_rwlock_rdlock(&M->rwlock);
	ctx = (struct context*)M->contexts[id % M->setsize];
	pthread_rwlock_unlock(&M->rwlock);

	contextRetain(ctx);
	return ctx;	
}

int ctxMgrGetContextNum() {
	return M->contextNum;
}

int ctxMgrInit(int setsize) {
	assert(M == NULL);
	M = ctxMgrCreate(setsize);
	return 0;
}

void ctxMgrUninit() {
	assert(M != NULL);
	ctxMgrRelease(M);
}

void ctxMgrStop() {
	uint32_t i;

	pthread_rwlock_wrlock(&M->rwlock);
	for (i=0; i < M->setsize; i++) {
		struct context* ctx = M->contexts[i];
		if (ctx) {
			if (contextRelease(ctx) == NULL) { 
				M->contexts[i] = NULL;
				M->contextNum--;
			}
		}
	}
	pthread_rwlock_unlock(&M->rwlock);
}

