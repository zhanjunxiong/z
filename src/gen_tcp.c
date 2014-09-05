#include "gen_tcp.h"

#include "anet.h"
#include "atomic_ops.h"
#include "event_msgqueue.h"
#include "eventloop.h"
#include "session.h"
#include "tcp_client.h"
#include "tcp_server.h"
#include "log.h"
#include "message.h"
#include "z.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>

struct genTcp {
	int id;
	int setsize;
	struct em *em;
	struct session **sessions;
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

static struct genTcp *M = NULL;

static int getIndex(int id) {
	return id & (M->setsize - 1);
	//return id % M->setsize;
}

int getReserveId() {
	int i;
	for (i=0; i < M->setsize; i++) {
		int id = AtomicAdd(&M->id, 1);
		if (id <= 0) {
			M->id = 1;
			id = 1;
		}
		struct session *session = M->sessions[getIndex(id)];
		if (CAS(&session, NULL, 1)) {
			fprintf(stderr, "id: %d\n", id);
			return id;
		}
	}
	fprintf(stderr, "session full\n");
	return -1;
}

static struct session *getSession(int id) {
	return M->sessions[getIndex(id)];
}

static void handleListen(struct socketRequest *req) {

	int fd = anetTcpServer(NULL, 
				req->u.listenRequest.port,
				req->u.listenRequest.addr,
				req->u.listenRequest.backlog);
	if (fd == ANET_ERR) {
		//reply->ud = -1;
		fprintf(stderr, "listen fail, addr(%s), port(%d)", 
						req->u.listenRequest.addr,
						req->u.listenRequest.port);
		return;
	} 

	struct aeEventLoop *eventloop = eventloopBase();
	struct session *session = newSession(eventloop, req->id, INVALID_FD);
	if (session == NULL) {
		//reply->ud = -2;
		return;
	}
	session->type = kSessionAcceptType;
	session->fd = fd;
	session->cb = &tcpServerCallback;
	session->protoType = req->u.listenRequest.protoType;
	session->port = req->u.listenRequest.port;
	snprintf(session->ip, IPADDR_LEN, "%s", req->u.listenRequest.addr);

	//sessionAccept(session, fd);

	/*
	struct socketMessage *reply = (struct socketMessage*)zcalloc(sizeof(*reply));
	reply->ud = 0;
	reply->id = session->id;

	zSend(NULL,
		0,
		req->from,
		req->sessionid,
		MSG_TYPE_RESPONSE,
		reply, 
		0);
	*/
}

static void handleStart(struct socketRequest *req) {
	int id = req->u.startRequest.id; 
	struct session *session = getSession(id);
	if (!session) {
		return;
	}

	assert(session->type != kSessionTypeNONE);

	session->from = req->from;
	if (session->type == kSessionAcceptType) {
		sessionAccept(session,  session->fd);
	} else if (session->type == kSessionSocketType) {
		sessionStart(session);
	} else if (session->type == kSessionConnectType) {
		if (sessionConnect(session) < 0) {
			//reply->ud = -2;
			return;
		}
	}
}

static void handleConnect(struct socketRequest *req) {
	struct aeEventLoop *eventloop = eventloopBase();
	struct session *session = newSession(eventloop, req->id, INVALID_FD);
	if (session == NULL) {
		//reply->ud = -1;
		return;
	}

	session->type = kSessionConnectType;
	session->cb = &tcpClientCallback;
	session->port = req->u.connectRequest.port;
	memcpy(session->ip, req->u.connectRequest.addr, IPADDR_LEN);
	session->protoType = req->u.connectRequest.protoType;
	/*
	if (sessionConnect(session) < 0) {
		//reply->ud = -2;
		return;
	}
	*/

	session->destId = session->id;
	session->from = req->from;
	struct tcpClient *ud = (struct tcpClient*)zcalloc(sizeof(*ud));
	sessionSetud(session, ud);


	/*
	struct socketMessage *reply = (struct socketMessage*)zcalloc(sizeof(*reply));

	reply->ud = 0;
	reply->id = session->id;

	zSend(NULL,
		NULL,
		req->from,
		req->sessionid,
		MSG_TYPE_RESPONSE,
		reply, 
		0);
	*/
}

static void handleSend(struct socketRequest *req) {
	int ret = 0;
	int i;
	for (i = 0; i < req->u.sendRequest.idNum; i++) {
		struct session *session = getSession(req->ids[i]); 
		if (session) {
			ret = sessionWrite(session, 
						req->u.sendRequest.data, 
						req->u.sendRequest.sz);
		} else {
			ret = -1;
			logInfo("not found session id(%d)", req->ids[i]);
		}
	}

	if (req->u.sendRequest.data) {
		zfree(req->u.sendRequest.data);
	}
}

static void handleClose(struct socketRequest *req) {
	int ret = 0;
	struct session* session = getSession(req->u.closeRequest.id); 
	if (session) {
		sessionForceClose(session); 
	} else {
		ret = -1;
		logInfo("not found session key(%s)", req->u.closeRequest.id);
	}
}

static void handleEvent(void *event, void *ud) {

	struct socketRequest *req = (struct socketRequest*)event;
	int type = req->type;
	switch(type){
		case kListen:
			handleListen(req);
			break;
		case kStart:
			handleStart(req);
			break;
		case kConnect:
			handleConnect(req);
			break;
		case kSend:
			handleSend(req);
			break;
		case kClose:
			handleClose(req);
			break;
		case kExit:
			break;
		default:
			break;
	}

	zfree(req);
}

struct session *newSession(struct aeEventLoop *eventloop, int id, int fd) {
	//int id = getReserveId();
	assert(id >= 0);
	if (id < 0) {
		return NULL;
	}
	struct session *session = sessionCreate(eventloop, id, fd);
	M->sessions[getIndex(id)] = session;
	return session;
}

void removeSession(int id) {
	struct session *session = getSession(id);
	if (session) {
		sessionRelease(session);
	}
	M->sessions[getIndex(id)] = NULL;
}


int genTcpListen(struct context *ctx, 
				int protoType,
				const char *addr,
				int port) {

	assert(ctx != NULL);
	assert(strlen(addr) <= NAME_MAX_LEN);

	struct socketRequest *req = (struct socketRequest*)zmalloc(sizeof(*req));
	int id = req->id = getReserveId(); 
	req->from = ctx->id;
	req->type = kListen;
	req->u.listenRequest.protoType = protoType;
	snprintf(req->u.listenRequest.addr, NAME_MAX_LEN, "%s", addr);
	req->u.listenRequest.port = port;
	req->u.listenRequest.backlog = SOMAXCONN;

	//int session = req->sessionid = contextNewsession(ctx);

	if(emPush(M->em, req) < 0) {
		zfree(req);
		return -1;
	}
	return id;
}

int genTcpStartSocket(struct context *ctx,
						int id) {
	struct socketRequest *req = (struct socketRequest*)zmalloc(sizeof(*req));
	req->from = ctx->id;
	req->type = kStart;
	req->u.startRequest.id = id;
	int session = req->sessionid = contextNewsession(ctx);
	if(emPush(M->em, req) < 0) {
		zfree(req);
		return -1;
	}
	return session;
}

int genTcpConnect(struct context *ctx,
				int protoType,
				const char *addr,
				int port,
				int retry) {

	assert(ctx != NULL);
	assert(strlen(addr) <= NAME_MAX_LEN);

	struct socketRequest *req = (struct socketRequest*)zmalloc(sizeof(*req));

	int id = req->id = getReserveId(); 
	req->from = ctx->id;
	req->type = kConnect;
	req->u.connectRequest.protoType = protoType;
	snprintf(req->u.connectRequest.addr, NAME_MAX_LEN, "%s", addr);
	req->u.connectRequest.port = port;

	//int session = req->sessionid = contextNewsession(ctx);

	if(emPush(M->em, req) < 0) {
		zfree(req);
		return -1;
	}
	return id;
	//return session;

}

int genTcpSend(struct context *ctx, 
				int ids[], 
				int idNum, 
				void *data, 
				size_t sz) {
	assert(ctx != NULL);

	if(ids == NULL) {
		idNum = 0;
	}

	struct socketRequest *req = (struct socketRequest*)
									zmalloc(sizeof(*req) + sizeof(int) *idNum);

	req->from = ctx->id;
	req->type = kSend;
	req->u.sendRequest.idNum = idNum;
	req->u.sendRequest.data = data;
	req->u.sendRequest.sz = sz;
	int session = req->sessionid = contextNewsession(ctx);

	int i;
	for (i = 0; i < idNum; i++) {
		req->ids[i] = ids[i];
	}

	if(emPush(M->em, req) < 0) {
		zfree(req);
		return -1;
	}
	return session;
}

int genTcpCloseSocket(struct context *ctx, 
						int id) {
	assert(ctx != NULL);

	struct socketRequest *req = (struct socketRequest*)zmalloc(sizeof(*req));
	req->from = ctx->id;
	req->type = kClose;
	req->u.closeRequest.id = id;
	int session = req->sessionid = contextNewsession(ctx);

	if(emPush(M->em, req) < 0) {
		zfree(req);
		return -1;
	}
	return session;
}

int genTcpInit(int setsize) {
	assert(M == NULL);

	struct aeEventLoop *eventLoop = eventloopBase();
	assert(eventLoop != NULL);

	M = (struct genTcp *)zmalloc(sizeof(*M));
	M->em = emCreate(eventLoop, GEN_TCP_QUEUE_MAX_LEN, handleEvent, NULL);
	assert(M->em != NULL);

	M->id = 1;

	setsize = nextpow2(setsize);
	M->setsize = setsize;
	M->sessions = (struct session**)zcalloc(sizeof(struct session*)*setsize);
	return 0;
}

void genTcpUninit() {
	assert(M != NULL);

	int i;
	for (i=0; i < M->setsize; i++) {
		if (M->sessions[i]) {
			sessionRelease(M->sessions[i]);
			M->sessions[i] = NULL;
		}
	}

	if (M->sessions) {
		zfree(M->sessions);
	}
	if (M->em) {
		emRelease(M->em);
	}
	zfree(M);
}

void genTcpStop() {
	struct socketRequest *req = (struct socketRequest*)zmalloc(sizeof(*req));
	req->type = kExit;
	if(emPush(M->em, req) < 0) {
		zfree(req);
	}
}

uint32_t buildSessionUd(struct session *session) {
	return session->id;

	//uint32_t ret = (session->id | ((session->destId & 0xFFFF) << 16));
	//return ret;
}


