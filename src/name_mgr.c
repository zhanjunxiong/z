#include "name_mgr.h"

#include "ctx_mgr.h"
#include "dict.h"
#include "zmalloc.h"

#include <string.h>
#include <stdio.h>
#include <pthread.h>

struct nameMgr {
	pthread_mutex_t lock;
	dict *nameDict;
};

struct name2id {
	char *name;
	int id;
};

static struct nameMgr *M = NULL;

static unsigned int _dictHashFunction(const void *key) {
    return dictGenHashFunction((char*)key, strlen((char*)key));
}

/*
static void *_dictKeyDup(void *privdata, const void *key) {
	return NULL;
}

static void *_dictStringDup(void *privdata, const void *key) {
	return NULL;
}
*/

static int _dictKeyCompare(void *privdata, const void *key1, const void *key2) {
	return (strcmp((const char *)key1, (const char *)key2) == 0);
}

/*
static void _dictKeyDestructor(void *privdata, void *key) {
}
*/

static void _dictValueDestructor(void *privdata, void *value) {
	struct name2id* name = (struct name2id*)value;
	zfree(name->name);
	zfree(name);
}

dictType nameDictType = {
	_dictHashFunction, // hash function
	NULL, // key dup
	NULL, // value dup
	_dictKeyCompare, // key compare
	NULL, // key destructor
	_dictValueDestructor,// value destructor
};

int nameMgrRegist(const char *name, int id) {
	struct context *ctx = ctxMgrGetContext(id);
	if (ctx == NULL) {
		return -2;
	}

 	int len = strlen(name);
    char *copy = (char *)zmalloc(len+1);
  	memcpy(copy, name, len);
    copy[len] = '\0';
  
	struct name2id *name2id = (struct name2id*)zmalloc(sizeof(*name2id));
	name2id->name = copy,
	name2id->id = id;

	pthread_mutex_lock(&M->lock);
	snprintf(ctx->name, 32, "%s", name);
	if (dictAdd(M->nameDict, name2id->name, name2id) == DICT_OK) {
		pthread_mutex_unlock(&M->lock);

		ctxMgrReleaseContext(ctx);
		return 0;
	}

	pthread_mutex_unlock(&M->lock);
	zfree(name2id->name);
	zfree(name2id);

	ctxMgrReleaseContext(ctx);
	return -1;
}

int nameMgrUnRegist(const char *name) {
	int ret = -1;
	
	int id = nameMgrGetId(name);
	if(id <= 0) {
		return ret;
	}
	
	pthread_mutex_lock(&M->lock);
	ret = dictDelete(M->nameDict, name);
	pthread_mutex_unlock(&M->lock);

	if (ret == 0) {
		struct context *ctx = ctxMgrGetContext(id);
		if (ctx) {
			pthread_mutex_lock(&M->lock);
			snprintf(ctx->name, 32, "c_%d", ctx->id);
			pthread_mutex_unlock(&M->lock);
		}
	}

	return ret;
}

int nameMgrGetId(const char *name) {
	struct name2id *name2id = NULL;	
	pthread_mutex_lock(&M->lock);
	name2id = (struct name2id *)dictFetchValue(M->nameDict, name);
	pthread_mutex_unlock(&M->lock);
	if (name2id) {
		return name2id->id;
	}
	return 0;
}

int nameMgrInit() {
	M = (struct nameMgr*)zmalloc(sizeof(*M));
	pthread_mutex_init(&M->lock, NULL);
	M->nameDict = dictCreate(&nameDictType, NULL);
	return 0;
}

void nameMgrUninit() {
	pthread_mutex_destroy(&M->lock);
	dictRelease(M->nameDict);
	zfree(M);
}


