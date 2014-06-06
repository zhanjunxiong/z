#include "ctx_mgr.h"

#include "context.h"
#include "queue.h"
#include "dict.h"
#include "zmalloc.h"

#include <pthread.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

struct ctxMgr {
	pthread_mutex_t lock;
	dict* ctxDict;
};

static struct ctxMgr* M = NULL;

static unsigned int _dictHashFunction(const void *key) {
	const char* key1 = (const char*)key;
    return dictGenHashFunction(key1, strlen(key1));
}

static int _dictKeyCompare(void *privdata, const void *key1, const void *key2) {
	return strcmp((const char*)key1, (const char*)key2) == 0;
}

static void _dictValueDestructor(void *privdata, void *value) {
	struct context* ctx = (struct context*)value;
	contextRelease(ctx);
}

dictType ctxDictType = {
	_dictHashFunction, // hash function
	NULL, // key dup
	NULL, // value dup
	_dictKeyCompare, // key compare
	NULL, // key destructor
	_dictValueDestructor // value destructor
};

// @return -1 加ctx 失败
// 			0 加ctx 成功
int ctxMgrAddContext(const char* name, struct context* ctx) {
	assert(M != NULL);

	int ret = 0;
	pthread_mutex_lock(&M->lock);
	if (dictAdd(M->ctxDict, (void*)name, ctx) == DICT_ERR) {
		ret = -1;
	}
	pthread_mutex_unlock(&M->lock);
	return ret;
}

int ctxMgrRemoveContext(const char* name) {
	int ret = 0;
	pthread_mutex_lock(&M->lock);
	ret = dictDelete(M->ctxDict, name);
	pthread_mutex_unlock(&M->lock);
	return ret;
}

int ctxMgrHasWork() {
	int hasWork = 0;
	dictIterator *di;
	dictEntry *de;
	pthread_mutex_lock(&M->lock);
	di = dictGetIterator(M->ctxDict);
	while((de = dictNext(di)) != NULL) {
		struct context* ctx = (struct context*)de->v.val;
		if (queueLength(ctx->queue) > 0) {
			hasWork = 1;
			break;
		}
	}
	dictReleaseIterator(di);
	pthread_mutex_unlock(&M->lock);
	return hasWork;
}

struct context* ctxMgrGetContext(const char* name) {
	assert(name != NULL);

	struct context* ctx = NULL;
	pthread_mutex_lock(&M->lock);
	ctx = (struct context*)dictFetchValue(M->ctxDict, name);
	pthread_mutex_unlock(&M->lock);
	return ctx;	
}

int ctxMgrInit() {
	assert(M == NULL);
	M = (struct ctxMgr*)zcalloc(sizeof(*M));
	assert(M != NULL);
	pthread_mutex_init(&M->lock, NULL);
	
	M->ctxDict = dictCreate(&ctxDictType, NULL);
	return 0;
}

void ctxMgrUninit() {
	dictRelease(M->ctxDict);
	pthread_mutex_destroy(&M->lock);
	zfree(M);
}

