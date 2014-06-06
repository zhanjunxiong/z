#include "tcp_client.h"

#include "ae.h"
#include "anet.h"
#include "dict.h"
#include "event_msgqueue.h"
#include "eventloop.h"
#include "session.h"
#include "zmalloc.h"
#include "protocol.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

struct tcpClientMgr {
	pthread_mutex_t lock;
	dict* clientDict;
};

static struct tcpClientMgr* M = NULL;

static void realConnect(struct tcpClient* tcpClient, int fd); 
static void onConnecting(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask); 

static int startConnect(struct tcpClient* tcpClient) {
	if (tcpClient->state != tcpClient_kConnecting) {
		return -1;
	}

	char err[256];
	int flag;
	int fd = anetTcpConnectEx(err, tcpClient->serverAddr, tcpClient->serverPort, &flag);
	if (fd == ANET_ERR) {
		return -2;
	}

    anetNonBlock(NULL,  fd);
    anetTcpNoDelay(NULL, fd);
    if (flag == 0) {
    	realConnect(tcpClient, fd);
    } else if (flag == 1) {
		aeCreateFileEvent(tcpClient->base, fd, AE_READABLE | AE_WRITABLE, onConnecting, tcpClient);
    } else {
		return -3;
	}
	return 0;
}

int _startConnect(struct aeEventLoop *eventLoop, long long id, void *clientData) {
	struct tcpClient* tcpClient = (struct tcpClient*)clientData;
	tcpClientStart(tcpClient);
	return 0;
}


static const uint32_t kMaxRetryDelayMs = 30*1000;
static void onReConnect(struct tcpClient* tcpClient) {
	if (tcpClient->retry == 1) {
		tcpClient->state = tcpClient_kConnecting;
		aeCreateTimeEvent(tcpClient->base, tcpClient->retryDelayMs/1000.0, _startConnect, tcpClient, NULL);
		tcpClient->retryDelayMs = 
			tcpClient->retryDelayMs < kMaxRetryDelayMs ? tcpClient->retryDelayMs * 2 :  kMaxRetryDelayMs;
	}
}

static void onClose(struct session* session) {
	struct tcpClient* tcpClient = (struct tcpClient*)session->owner.client;
	if (tcpClient->onNetCallback.onClose) {
		tcpClient->onNetCallback.onClose(session);
	}
	onReConnect(tcpClient);
}

static void onConnecting(struct aeEventLoop *eventLoop, int fd, void *clientData, int mask) {
	struct tcpClient* tcpClient = (struct tcpClient*)clientData;
	int error = 0;
	socklen_t len = sizeof(int);

	aeDeleteFileEvent(tcpClient->base, fd, AE_READABLE | AE_WRITABLE);
	if ( (0 == getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len))) {
		if (0 == error){
			realConnect(tcpClient, fd);
			return;
		}
	} 

	close(fd);
	onReConnect(tcpClient);
}

static void realConnect(struct tcpClient* tcpClient, int fd) {
	if (!tcpClient->session) {
		tcpClient->session = sessionCreate();
	}

	struct session* session = tcpClient->session;
	assert(session != NULL);

	session->base = tcpClient->base;
	session->mask = SESSION_READABLE;
	session->cfd = fd;
	
	session->onNetCallback = tcpClient->onNetCallback;
	session->onNetCallback.onClose = onClose;

	session->state = kConnected;
	session->owner.client = tcpClient;
	session->privateData = tcpClient->privateData;
	session->protoName = tcpClient->protoName;

	sessionUpdateEvent(session);
	tcpClient->state = tcpClient_kConnected;

	if (session->onNetCallback.onConnection) {
		session->onNetCallback.onConnection(session);
	}


}

static void sendMessage(void* msg, void* ud) {
	struct tcpClient* client = (struct tcpClient*)ud;
	struct tcpMessage* smsg = (struct tcpMessage*)msg;

	struct session* session = client->session;
	if (session) {
		if (smsg->type == TCP_MSG_TYPE_SEND) {
			sessionWrite(session, smsg->data, smsg->sz);
		} else if (smsg->type == TCP_MSG_TYPE_CLOSE) {
			sessionForceClose(session); 
		}
	}

	zfree(smsg);
}


struct tcpClient* tcpClientCreate(const char* name, const char* serverAddr, uint32_t serverPort) {

	struct tcpClient* client = (struct tcpClient*)zcalloc(sizeof(*client));
	client->name = zstrdup(name);

	int ret = -1;
	pthread_mutex_lock(&M->lock);
	if (dictAdd(M->clientDict, client->name, client) == DICT_OK) {
		ret = 0;
	}
	pthread_mutex_unlock(&M->lock);
	if (ret < 0) {
		zfree(client->name);
		zfree(client);
		return NULL;
	}

	client->base =  eventloopBase();
	client->serverAddr = zstrdup(serverAddr);
	client->serverPort = serverPort;
	client->state = tcpClient_kDisconnected;
	client->em = emCreate(client->base, 2, sendMessage, client);

	return client;
}

void tcpClientRelease(struct tcpClient* tcpClient) {
	if (tcpClient->name) {
		zfree(tcpClient->name);
	}
	if (tcpClient->serverAddr) {
		zfree(tcpClient->serverAddr);
	}
	if (tcpClient->session) {
		sessionRelease(tcpClient->session);
	}
	if (tcpClient->protoName) {
		zfree(tcpClient->protoName);
	}
	zfree(tcpClient);
}

int tcpClientStart(struct tcpClient* tcpClient) {
	tcpClient->state = tcpClient_kConnecting;
	return startConnect(tcpClient);
}

void tcpClientStop(struct tcpClient* tcpClient) {
	tcpClient->state = tcpClient_kDisconnected;
	if (tcpClient->session) {
		sessionRemoveEvent(tcpClient->session);
		tcpClient->session = NULL;
	}

	pthread_mutex_lock(&M->lock);
	dictDelete(M->clientDict, tcpClient->name);
	pthread_mutex_unlock(&M->lock);
}

void tcpClientConnectionCallback(struct tcpClient* client, connectionCallback cb) {
	client->onNetCallback.onConnection = cb;
}

void tcpClientCloseCallback(struct tcpClient* client, closeCallback cb) {
	client->onNetCallback.onClose = cb;
}

void tcpClientWriteCompleteCallback(struct tcpClient* client, writeCompleteCallback cb) {
	client->onNetCallback.onWriteComplete = cb;
}

void tcpClientHightWaterMarkCallback(struct tcpClient* client, hightWaterMarkCallback cb) {
	client->onNetCallback.onHightWaterMark = cb;
}

void tcpClientMessageCallback(struct tcpClient* client, messageCallback cb) {
	client->onNetCallback.onMessage = cb;
}

void tcpClientProtoMessageCallback(struct tcpClient* client, protoMessageCallback cb) {
	client->onNetCallback.onProtoMessage = cb;
}

void tcpClientPrivateData(struct tcpClient* client, void* privateData) {
	client->privateData = privateData;
}


void tcpClientSetProto(struct tcpClient* client, const char* proto) {
	client->onNetCallback.onMessage = defaultMessageCallback;
	client->protoName = zstrdup(proto);
}

struct tcpClient* getTcpClient(const char* name) {
	assert(name != NULL);

	struct tcpClient* client = NULL;

	pthread_mutex_lock(&M->lock);
	client = (struct tcpClient*)dictFetchValue(M->clientDict, name);
	pthread_mutex_unlock(&M->lock);

	return client;	
}


int tcpClientSend(const char* clientName, int type, void* data, size_t sz) {
	struct tcpClient* client = getTcpClient(clientName);
	struct tcpMessage* msg = (struct tcpMessage*)zcalloc(sizeof(*msg));
	msg->type = type;
	msg->data = data;
	msg->sz = sz;
	emPush(client->em, msg);
	return 0;
}

static unsigned int _dictHashFunction(const void *key) {
    return dictGenHashFunction(key, strlen((const char*)key));
}

static int _dictKeyCompare(void *privdata, const void *key1, const void *key2) {
	return strcmp((const char*)key1, (const char*)key2) == 0;
}

static void _dictValueDestructor(void *privdata, void *value) {
}

dictType clientDictType = {
	_dictHashFunction, // hash function
	NULL, // key dup
	NULL, // value dup
	_dictKeyCompare, // key compare
	NULL,
	_dictValueDestructor // value destructor
};


int tcpClientInit() {
	assert(M == NULL);
	M = (struct tcpClientMgr*)zcalloc(sizeof(*M));
	pthread_mutex_init(&M->lock, NULL);
	M->clientDict = dictCreate(&clientDictType, NULL);
	return 0;

}

void tcpClientUninit() {
	pthread_mutex_destroy(&M->lock);
	dictRelease(M->clientDict);
	zfree(M);
}



