#include "tcp_server.h" 

#include "ae.h"
#include "anet.h"
#include "buffer.h"
#include "eventloop.h"
#include "event_msgqueue.h"
#include "message.h"
#include "define.h"
#include "dict.h"
#include "session.h"
#include "protocol.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

struct tcpServerMgr {
	pthread_mutex_t lock;
	dict* srvDict;
};

static struct tcpServerMgr* M = NULL;

struct sessionKey {
	uint32_t id;
};

static unsigned int _dictHashFunction(const void *key) {
	struct sessionKey* sessionKey = (struct sessionKey*)key;
	return sessionKey->id;
}

static int _dictKeyCompare(void *privdata, const void *key1, const void *key2) {
	struct sessionKey* sessionKey1 = (struct sessionKey*)key1;
	struct sessionKey* sessionKey2 = (struct sessionKey*)key2;
	return sessionKey1->id == sessionKey2->id;
}

static void _dictKeyDestructor(void *privdata, void *key) {
	zfree(key);
}

static void _dictValueDestructor(void *privdata, void *value) {
	struct session* session = (struct session*)value;
	sessionRelease(session);
}

dictType sessionDictType = {
	_dictHashFunction, // hash function
	NULL, // key dup
	NULL, // value dup
	_dictKeyCompare, // key compare
	_dictKeyDestructor, // key destructor
	_dictValueDestructor // value destructor
};
static void sendToSession(void* msg, void* ud) {
	struct tcpServer* srv = (struct tcpServer*)ud;
	struct tcpMessage* smsg = (struct tcpMessage*)msg;

	struct sessionKey key;
	key.id = smsg->id;
	void* value = dictFetchValue(srv->sessions, &key);
	struct session* session = (struct session*)value;
	if (session) {
		if (smsg->type == TCP_MSG_TYPE_SEND) {
			sessionWrite(session, smsg->data, smsg->sz);
		} else if (smsg->type == TCP_MSG_TYPE_CLOSE) {
			sessionForceClose(session); 
		}
	}

	zfree(smsg);
}

static void onClose(struct session* session) {
	struct tcpServer* srv = session->owner.srv;
	if (srv) {
		srv->onNetCallback.onClose(session);
	}
}

static void onAccept(struct aeEventLoop *base, int fd, void *ud, int mask) {
	struct tcpServer* srv = (struct tcpServer*) ud;
	assert(srv != NULL);
	
	struct session* session = sessionCreate();
	if (!session)
		return;

	char neterr[256];
	int cfd = anetTcpAccept(neterr, srv->listenfd, session->cip, &session->cport);
	if (cfd == AE_ERR) {
		sessionRelease(session);
		return;
	}

	anetNonBlock(NULL, cfd);
	anetTcpNoDelay(NULL, cfd);

	session->cfd = cfd;
	session->state = kConnected;

	session->base = srv->base;

	session->onNetCallback = srv->onNetCallback;
	session->onNetCallback.onClose = onClose;

	session->privateData = srv->privateData;
	session->owner.srv = srv;
	session->protoName = srv->protoName;

	if (session->onNetCallback.onConnection){
		session->onNetCallback.onConnection(session);
	}

	sessionUpdateEvent(session);

	struct sessionKey* key = (struct sessionKey*)zmalloc(sizeof(*key));
	key->id = session->id;

	dictAdd(srv->sessions, key, session);
}

struct tcpServer* tcpServerCreate(const char* srvName,
									const char* listenAddr, 
									int listenPort) {
	assert(M != NULL);
	struct tcpServer* srv = (struct tcpServer*)zcalloc(sizeof(*srv));
	assert(srv != NULL);

	struct aeEventLoop* base = eventloopBase();
	assert(base != NULL);

	srv->name = zstrdup(srvName);
	srv->listenAddr = zstrdup(listenAddr);
	srv->listenPort = listenPort;
	srv->listenfd = -1;
	srv->base = base;
	srv->sessions = dictCreate(&sessionDictType, srv);
	srv->em = emCreate(srv->base, 2, sendToSession, srv);

	return srv;
}

void tcpServerRelease(struct tcpServer* srv) {
	// 开启状态
	if (srv->state == 1) {
		// 关闭掉
		tcpServerStop(srv);
	}
	if (srv->name) {
		zfree(srv->name);
	}
	if (srv->listenAddr) {
		zfree(srv->listenAddr);
	}
	if (srv->protoName) {
		zfree(srv->protoName);
	}
	if (srv->sessions) {
		dictRelease(srv->sessions);
	}
	if (srv->em) {
		emRelease(srv->em);
	}
	zfree(srv);
}

int tcpServerStart(struct tcpServer* srv) {
	srv->listenfd = anetTcpServer(NULL, srv->listenPort, srv->listenAddr);
	assert(srv->listenfd != ANET_ERR);
    anetNonBlock(NULL,  srv->listenfd);
    anetTcpNoDelay(NULL, srv->listenfd);

	aeCreateFileEvent(srv->base,
			srv->listenfd,
			AE_READABLE,
			onAccept,
			srv);
	
	// 开启
	srv->state = 1;
	
	pthread_mutex_lock(&M->lock);
	dictAdd(M->srvDict, srv->name, srv);
	pthread_mutex_unlock(&M->lock);
	return 0;
}

void tcpServerStop(struct tcpServer* srv) {
	if (srv->base) {
		aeDeleteFileEvent(srv->base, srv->listenfd, AE_READABLE);
	}
	if (srv->listenfd) {
		close(srv->listenfd);
	}

	dictIterator *di;
	dictEntry *de;
	di = dictGetIterator(srv->sessions);
	while((de = dictNext(di)) != NULL) {
		sessionRemoveEvent((struct session*)de->v.val);
	}
	dictReleaseIterator(di);
	// 关闭
	srv->state = 2;

	//pthread_mutex_lock(&M->lock);
	//dictDeleteNoFree(M->srvDict, srv->name);
	//pthread_mutex_unlock(&M->lock);
}

void tcpServerConnectionCallback(struct tcpServer* srv, connectionCallback cb) {
	srv->onNetCallback.onConnection = cb;
}

void tcpServerCloseCallback(struct tcpServer* srv, closeCallback cb) {
	srv->onNetCallback.onClose = cb;
}

void tcpServerWriteCompleteCallback(struct tcpServer* srv, writeCompleteCallback cb) {
	srv->onNetCallback.onWriteComplete = cb;
}

void tcpServerHightWaterMarkCallback(struct tcpServer* srv, hightWaterMarkCallback cb) {
	srv->onNetCallback.onHightWaterMark = cb;
}

void tcpServerMessageCallback(struct tcpServer* srv, messageCallback cb) {
	srv->onNetCallback.onMessage = cb;
}

void tcpServerProtoMessageCallback(struct tcpServer* srv, protoMessageCallback cb) {
	srv->onNetCallback.onProtoMessage = cb;
}

void tcpServerPrivateData(struct tcpServer* srv, void* privateData) {
	srv->privateData = privateData;
}
void tcpServerSetProto(struct tcpServer* srv, const char* proto) {
	srv->onNetCallback.onMessage = defaultMessageCallback;
	srv->protoName = zstrdup(proto);
}

static unsigned int _srvdictHashFunction(const void *key) {
    return dictGenHashFunction(key, strlen((const char*)key));
}

static int _srvdictKeyCompare(void *privdata, const void *key1, const void *key2) {
	return strcmp((const char*)key1, (const char*)key2) == 0;
}

static void _srvdictValueDestructor(void *privdata, void *value) {
	//struct tcpServer* srv = (struct tcpServer*)value;
	//tcpServerRelease(srv);
}

dictType srvDictType = {
	_srvdictHashFunction, // hash function
	NULL, // key dup
	NULL, // value dup
	_srvdictKeyCompare, // key compare
	NULL,
	_srvdictValueDestructor // value destructor
};

struct tcpServer* getTcpServer(const char* name) {
	assert(name != NULL);

	struct tcpServer* srv = NULL;

	pthread_mutex_lock(&M->lock);
	srv = (struct tcpServer*)dictFetchValue(M->srvDict, name);
	pthread_mutex_unlock(&M->lock);

	return srv;	
}

int tcpServerSend(const char* srvName, uint32_t id, int type, void* data, size_t sz){
	
	struct tcpServer* srv = getTcpServer(srvName);
	struct tcpMessage* msg = (struct tcpMessage*)zcalloc(sizeof(*msg));
	msg->id = id;
	msg->type = type;
	msg->data = data;
	msg->sz = sz;
	emPush(srv->em, msg);
	return 0;
}

int tcpServerInit() {
	assert(M == NULL);
	M = (struct tcpServerMgr*)zcalloc(sizeof(*M));
	pthread_mutex_init(&M->lock, NULL);
	M->srvDict = dictCreate(&srvDictType, NULL);
	return 0;
}

void tcpServerUninit() {

	dictIterator *di;
	dictEntry *de;
	di = dictGetIterator(M->srvDict);
	while((de = dictNext(di)) != NULL) {
		tcpServerRelease((struct tcpServer*)de->v.val);
	}
	dictReleaseIterator(di);

	dictRelease(M->srvDict);
	pthread_mutex_destroy(&M->lock);
	zfree(M);
}


