#include "tcp.h"

#include "anet.h"
#include "atomic_ops.h"
#include "event_msgqueue.h"
#include "eventloop.h"
#include "socket.h"
#include "z.h"
#include "zmalloc.h"

#include <assert.h>
#include <errno.h>

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>


struct tcp {
	int allocId;
	int setsize;
	struct em *em;
	struct socket *sockets;
};

enum tcpRequestE {
	kTcpSocketListen,
	kTcpSocketStart,
	kTcpSocketConnect,
	kTcpSocketAsyncConnect,
	kTcpSocketClose,
	kTcpSocketSend,
	kTcpStop,
};

struct listenTcpRequest {
	char addr[32];
	int port;
	int protoType;
	int backlog;
};

struct connectTcpRequest {
	char addr[32];
	int port;
	int protoType;
};

struct sendTcpRequest {
	size_t sz;
	void *data;
	int idNum;
};


struct tcpRequest {
	int from;
	enum tcpRequestE type;
	union {
		struct listenTcpRequest listenTcpRequest;
		struct connectTcpRequest connectTcpRequest;
		struct sendTcpRequest sendTcpRequest;
	} u;
	int ids[];
};

static uint32_t nextpow2(uint32_t num) {
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return ++num;
}


static struct tcp *T = NULL;

void tcpError(int to, int id, char *err) {
	char *data = NULL;
	size_t errLen = 0;
	if (err) {
		errLen = strlen(err);
		data = (char *)zmalloc(errLen + 1);
		memcpy(data, err,  errLen);
	}

	zSendx(NULL, 
			0,
			to,
			0,
			((MSG_TYPE_SOCK_ERROR & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
			data, 
			errLen,
			id);
}

static int getIndex(int id) {
	return id & (T->setsize - 1);
}

struct socket *tcpGetSocket(int id) {
	struct socket *socket = &(T->sockets[getIndex(id)]);
	if (socket->id == id) {
		return socket;
	}
	return NULL;
}

int tcpRecoverId(int id) {
	if (id < 0) return -1;

	struct socket *socket = &(T->sockets[getIndex(id)]);
	assert(socket->state == kSocketDisconnected);

	socket->type = kSocketNoneType;
	return 0;
}

int tcpGetReserveId() {
	int i;
	for (i=0; i < T->setsize; i++) {
		int id = AtomicAdd(&(T->allocId), 1);
		if (id < 0) {
			id = __sync_and_and_fetch(&(T->allocId), 0x7FFFFFFF);
		}
		struct socket *s = &(T->sockets[getIndex(id)]);
		if (s->type == kSocketNoneType) {
			if (CAS(&(s->type), kSocketNoneType, kReserveType)) {
				s->id = id;
				s->fd = -1;
				return id;
			} else {
				// retry
				--i;
			}
		}
	}

	assert(0);
	return -1;
}

static void handleTcpSocketListen(struct tcpRequest *req) {

	char err[ANET_ERR_LEN];
	int id = req->ids[0];
	int fd = anetTcpServer(err, 
				req->u.listenTcpRequest.port,
				req->u.listenTcpRequest.addr,
				req->u.listenTcpRequest.backlog);
	if (fd == ANET_ERR) {
		char str[1024];
		snprintf(str, 1024, "listen on (%s:%d) fail, %s", 
							req->u.listenTcpRequest.addr,
							req->u.listenTcpRequest.port,
							err);

		tcpError(req->from, id, str); 
		return;
	} 

	struct socket *socket = tcpGetSocket(id);
	if (socket == NULL) {
		char str[1024];
		snprintf(str, 1024, "listen on (%s:%d) fail, id not found", 
							req->u.listenTcpRequest.addr,
							req->u.listenTcpRequest.port);

		tcpError(req->from, id, str); 
		return;
	}

	socket->state = kSocketConnecting; 
	socket->type = kAcceptType;
	socket->fd = fd;
	socket->protoType = req->u.listenTcpRequest.protoType;
}

static void handleTcpSocketStart(struct tcpRequest *req) {
	int id = req->ids[0];
	struct socket *socket = tcpGetSocket(id);
	
	if (socket == NULL) {
		char str[1024];
		snprintf(str, 1024, "start sockfd (%d) fail, id not found", 
							id);

		tcpError(req->from, id, str); 
		return;
	}

	if (socket->state != kSocketConnecting) {
		char str[1024];
		snprintf(str, 1024, "start sockfd (%d) fail, state is(%d)", 
							id,
							socket->state);

		tcpError(req->from, id, str); 
		return;
	}

	socket->from = req->from;
	int ret = -1;
	if (socket->type == kAcceptType) {
		ret = socketAccept(socket);
	} else if (socket->type == kSocketType || 
				socket->type == kConnectType) {
		ret = socketStart(socket);
	}

	if (ret < 0) {
		socketStop(socket);

		char str[1024];
		snprintf(str, 1024, "start sockfd (%d) fail", 
							id);
		tcpError(req->from, id, str); 
		return;
	}
}

static void handleTcpSocketConnect(struct tcpRequest *req) {
	int id = req->ids[0];
	struct socket *socket = tcpGetSocket(id);

	if (socket == NULL) {
		char str[1024];
		snprintf(str, 1024, "connect sockfd (%d) fail, id not found", 
							id);

		tcpError(req->from, id, str); 
		return;
	}


	char err[ANET_ERR_LEN];
	int fd = anetTcpConnect(err, 
			req->u.connectTcpRequest.addr, 
			req->u.connectTcpRequest.port);
	if (fd == ANET_ERR) {
		char str[1024];
		snprintf(str, 1024, "connect (%s:%d) fail, %s", 
							req->u.connectTcpRequest.addr,
							req->u.connectTcpRequest.port,
							err);

		tcpError(req->from, id, str); 
		return;
	}

	socket->type = kConnectType;
	socket->protoType = req->u.connectTcpRequest.protoType;
	socket->state = kSocketConnecting; 
	socket->fd = fd;
}

static void handleTcpSocketAsyncConnect(struct tcpRequest *req) {
	int id = req->ids[0];
	struct socket *socket = tcpGetSocket(id);
	if (socket == NULL) {
		char str[1024];
		snprintf(str, 1024, "asyncconnect sockfd (%d) fail, id not found", 
							id);

		tcpError(req->from, id, str); 
		return;
	}

	socket->from = req->from;

	char neterr[ANET_ERR_LEN];
	int fd = anetTcpNonBlockConnect(neterr, 
			req->u.connectTcpRequest.addr, 
			req->u.connectTcpRequest.port);

	if (fd == ANET_ERR ||
		errno != EINPROGRESS) {

		tcpRecoverId(id);

		char err[1024];
		snprintf(err, 1024, "connect fail, addr(%s:%d) %s", 
						req->u.connectTcpRequest.addr,
						req->u.connectTcpRequest.port,
						neterr);
		tcpError(req->from, socket->id, err);
		return;
	}

	int ret = socketConnect(socket); 
	if (ret < 0) {
		tcpRecoverId(id);

		char err[1024];
		snprintf(err, 1024, "connect fail, addr(%s:%d) create event fail", 
						req->u.connectTcpRequest.addr,
						req->u.connectTcpRequest.port
						);
		tcpError(req->from, socket->id, err);
		return;
	}

	socket->state = kSocketConnecting; 
	socket->fd = fd;
	socket->type = kConnectType;
	socket->protoType = req->u.connectTcpRequest.protoType;
}

static void handleTcpSocketClose(struct tcpRequest *req) {
	int id = req->ids[0];
	struct socket *socket = tcpGetSocket(id);
	if (socket == NULL) {
		char str[1024];
		snprintf(str, 1024, "close sockfd (%d) fail, id not found", 
							id);

		tcpError(req->from, id, str); 
		return;
	}


	socketForceClose(socket);
}

static void handleTcpSocketSend(struct tcpRequest *req) {
	void *data = req->u.sendTcpRequest.data; 
	size_t sz = req->u.sendTcpRequest.sz;
	int idNum = req->u.sendTcpRequest.idNum;
	if (data == NULL) {
		return;
	}

	int i;
	for (i = 0; i < idNum; i++) {
		int id = req->ids[i];
		struct socket *socket = tcpGetSocket(id);
		if (socket == NULL) {
			char str[64];
			snprintf(str, 64, "send sockfd (%d) fail, id not found", 
							id);

			tcpError(req->from, id, str); 
		} else {
			socketWrite(socket, 
						data,
						sz);
		}
	}

	if (data) {
		zfree(data);
	}
}

static void handleEvent(void *event, void *ud) {
	
	struct tcpRequest *req = (struct tcpRequest *)event;
	int type = req->type;
	switch(type){
		case kTcpSocketListen:
			handleTcpSocketListen(req);
			break;
		case kTcpSocketStart:
			handleTcpSocketStart(req);
			break;
		case kTcpSocketConnect:
			handleTcpSocketConnect(req);
			break;
		case kTcpSocketAsyncConnect:
			handleTcpSocketAsyncConnect(req); 
			break;
		case kTcpSocketClose:
			handleTcpSocketClose(req);
			break;
		case kTcpSocketSend:
			handleTcpSocketSend(req);
			break;
		case kTcpStop:
			eventloopStop(); 
			break;
		default:
			break;
	}

	zfree(req);
}

static int tcpPostRequest(int id, struct tcpRequest *req) {
	if (emPush(T->em, req) < 0) {
		zfree(req);
		// TODO: recover id
		tcpRecoverId(id); 
		return -1;
	}
	return id;
}


int tcpSocketListen(struct context *ctx,
				int protoType,
				const char *addr,
				int port) {

	assert((ctx != NULL) && (addr != NULL));

	struct tcpRequest *req = (struct tcpRequest*)zmalloc(sizeof(*req) + sizeof(int));
	int id = tcpGetReserveId();
	req->from = ctx->id;
	req->type = kTcpSocketListen;
	req->u.listenTcpRequest.protoType = protoType;
	snprintf(req->u.listenTcpRequest.addr, 32, "%s", addr); 
	req->u.listenTcpRequest.port = port;
	req->u.listenTcpRequest.backlog = SOMAXCONN;
	req->ids[0] = id;

	return tcpPostRequest(id, req);
}

int tcpSocketStart(struct context *ctx,
					int id) {

	struct tcpRequest *req = (struct tcpRequest *)zmalloc(sizeof(*req) + sizeof(int));
	req->from = ctx->id;
	req->type = kTcpSocketStart;
	req->ids[0] = id;

	return tcpPostRequest(id, req);
}

int tcpSocketConnect(struct context *ctx,
					int protoType,
					const char *addr,
					int port) {

	struct tcpRequest *req = (struct tcpRequest *)zmalloc(sizeof(*req) + sizeof(int));
	int id = tcpGetReserveId();
	req->ids[0] = id;
	req->type = kTcpSocketConnect;
	req->from = ctx->id;
	req->u.connectTcpRequest.protoType = protoType;
	snprintf(req->u.connectTcpRequest.addr, 32, "%s", addr);
	req->u.connectTcpRequest.port = port;

	return tcpPostRequest(id, req);
}

int tcpSocketAsyncConnect(struct context *ctx,
					int protoType,
					const char *addr,
					int port) {
	struct tcpRequest *req = (struct tcpRequest *)zmalloc(sizeof(*req) + sizeof(int));
	int id = tcpGetReserveId();
	req->ids[0] = id;
	req->type = kTcpSocketAsyncConnect;
	req->from = ctx->id;
	req->u.connectTcpRequest.protoType = protoType;
	snprintf(req->u.connectTcpRequest.addr, 32, "%s", addr);
	req->u.connectTcpRequest.port = port;

	return tcpPostRequest(id, req);
}

int tcpSocketClose(struct context *ctx,
					int id) {
	struct tcpRequest *req = (struct tcpRequest *)zmalloc(sizeof(*req) + sizeof(int));
	req->from = ctx->id;
	req->type = kTcpSocketClose;
	req->ids[0] = id;

	return tcpPostRequest(id, req);
}

int tcpSocketSend(struct context *ctx, 
				int ids[], 
				int idNum, 
				void *data, 
				size_t sz) {
	assert(ctx != NULL);

	if(ids == NULL) {
		idNum = 0;
	}

	struct tcpRequest *req = (struct tcpRequest *)
								zmalloc(sizeof(*req) + sizeof(int) * idNum);

	req->type = kTcpSocketSend,
	req->from = ctx->id;
	req->u.sendTcpRequest.idNum = idNum;
	req->u.sendTcpRequest.data = data;
	req->u.sendTcpRequest.sz = sz;

	int i;
	for (i = 0; i < idNum; i++) {
		req->ids[i] = ids[i];
	}

	return tcpPostRequest(-1, req);
}

void tcpStop() {
	struct tcpRequest *req = (struct tcpRequest *)zmalloc(sizeof(*req));
	req->type = kTcpStop;

	tcpPostRequest(-1, req);
}

int tcpInit(int setsize) {
	struct aeEventLoop *eventLoop = eventloopBase();
	assert(eventLoop != NULL);

	T = (struct tcp *)zmalloc(sizeof(*T));
	T->allocId = 1;

	setsize = nextpow2(setsize);
	T->setsize = setsize;
	T->em = emCreate(eventLoop, 1024, handleEvent, NULL);
	T->sockets = (struct socket *)zmalloc(sizeof(struct socket) * setsize);
	int i;
	for (i=0; i < setsize; i++) {
		socketInit(&T->sockets[i]);
	}
	return 0;
}

void tcpUninit() {
	int i;
	for (i=0; i < T->setsize; i++) {
		socketUninit(&T->sockets[i]);
	}

	if (T->sockets) {
		zfree(T->sockets);
	}
	if (T->em) {
		emRelease(T->em);
	}
	zfree(T);
}


