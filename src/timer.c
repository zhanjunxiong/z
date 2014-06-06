#include "timer.h"

#include "ae.h"
#include "dict.h"
#include "event_msgqueue.h"
#include "z.h"

#include <assert.h>
#include <pthread.h>

struct timer {
	struct aeEventLoop* base;
	struct em* em;
	struct dict* timerDict;
	uint32_t sn;
	pthread_mutex_t lock;
};

struct timerKey {
	uint32_t sn;
};

struct timerValue {
	long long id;
};

static struct timer* T = NULL;

struct timerEvent {
	uint32_t sn;
	char to[CONTEXT_NAME_MAX_LEN];
	union {
		uint32_t session;
		uint32_t sn;
	}value;
	int time;
	// 0 表示 一次定时器
	// 1 表示 循环定时器
	int noMore;
	// 
	// 0 加定时器
	// 1 取消定时器
	// 2 stop timer
	int type; 
};

static uint32_t timerNewSn() {
	uint32_t sn;
	pthread_mutex_lock(&T->lock);
	sn = ++T->sn & 0x7fffffff;
	pthread_mutex_unlock(&T->lock);
	return sn;
}

void timerCancel(uint32_t sn) {
	struct timerEvent* event = (struct timerEvent*)zmalloc(sizeof(*event));
	event->value.sn = sn;
	event->type = 1;
	if (emPush(T->em, event) < 0) {
		zfree(event);
	}
}

int timerTimeout(const char* to, int time, uint32_t session, int noMore) {
	if (time == 0) {
		if (zSend(NULL, "", to, session, MSG_TYPE_RESPONSE, NULL, 0) < 0) {
			return -1;
		}
	} else {
		struct timerEvent* event = (struct timerEvent*)zcalloc(sizeof(*event));
		memcpy(event->to, to, strlen(to));
		event->value.session = session;
		event->noMore = noMore;
		event->type = 0;
		event->time = time;
		if (emPush(T->em, event) < 0 ) {
			zfree(event);
			return -1;
		}
		event->sn = timerNewSn();
		return event->sn;
	}

	return 0;
}

void timerStop() {
	assert(T != NULL);
	struct timerEvent* event = (struct timerEvent*)zmalloc(sizeof(*event));
	event->type = 2;
	if (emPush(T->em, event) < 0) {
		fprintf(stderr, "stop timer fail\n");
		zfree(event);
	}
}

void timerMain() {
	assert(T != NULL);
	aeMain(T->base);
}

void eventFinalizerProc(struct aeEventLoop *eventLoop, void *clientData) {
	zfree(clientData);
}

static int timeProc(struct aeEventLoop *eventLoop, long long id, void *clientData) {
	struct timerEvent* event = (struct timerEvent*)clientData;
	if (zSend(NULL, "", event->to, event->value.session, MSG_TYPE_RESPONSE, NULL, 0) < 0) {
		return AE_NOMORE;
	}	

	if (event->noMore == 0) {
		return AE_NOMORE;
	}
	return 0;
}

static void emCallback(void* msg, void* ud) {
	struct timerEvent* event = (struct timerEvent*)msg;

	if (event->type == 0) {
		long long id = aeCreateTimeEvent(T->base, event->time,
			timeProc,
			event,
			eventFinalizerProc);

		struct timerKey* key = (struct timerKey*)zmalloc(sizeof(*key));
		key->sn = event->sn;

		struct timerValue* value = (struct timerValue*)zmalloc(sizeof(*value));
		value->id = id;
		dictAdd(T->timerDict, key, value);
	} else if (event->type == 1){
		struct timerKey key;
		key.sn = event->value.sn;

		struct timerValue* value = (struct timerValue*)dictFetchValue(T->timerDict, &key);
		if (value) {
			aeDeleteTimeEvent(T->base, value->id);
		}
		// free event
		zfree(event);
	} else {

		aeStop(T->base);
		// free event
		zfree(event);
	}
}

static unsigned int _dictHashFunction(const void *key) {
	struct timerKey* timerKey = (struct timerKey*)key;
	return timerKey->sn;
}

static int _dictKeyCompare(void *privdata, const void *key1, const void *key2) {
	struct timerKey* timerKey1 = (struct timerKey*)key1;
	struct timerKey* timerKey2 = (struct timerKey*)key2;
	return timerKey1->sn == timerKey2->sn;
}

static void _dictKeyDestructor(void *privdata, void *key) {
	zfree(key);
}

static void _dictValueDestructor(void *privdata, void *value) {
	zfree(value);
}

dictType timerDictType = {
	_dictHashFunction, // hash function
	NULL, // key dup
	NULL, // value dup
	_dictKeyCompare, // key compare
	_dictKeyDestructor, // key destructor
	_dictValueDestructor,// value destructor
};

int timerInit() {
	assert(T == NULL);
	T = (struct timer*)zmalloc(sizeof(*T));
	T->base = aeCreateEventLoop();
	T->em = emCreate(T->base, 128, emCallback, NULL);
	T->timerDict = dictCreate(&timerDictType, NULL);
	T->sn = 0;
	pthread_mutex_init(&T->lock, NULL); 
	return 0;
}

void timerUninit() {
	assert(T != NULL);
	emRelease(T->em);
	dictRelease(T->timerDict);
	pthread_mutex_destroy(&T->lock); 
	aeDeleteEventLoop(T->base);
	zfree(T);
}

