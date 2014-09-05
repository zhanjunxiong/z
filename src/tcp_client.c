#include "tcp_client.h"

#include "ae.h"
#include "gen_tcp.h"
#include "session.h"
#include "z.h"
#include "zmalloc.h"

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

static const int kRetryState = 1;
static const int kMaxRetryDelayMs = 30*1000;
static const int kMinRetryDelayMs = 1*1000;
static bool isReConnect(struct tcpClient *cli) {
	if (cli->retry == kRetryState) {
		return true;
	}
	return false;
}

static int _startConnect(struct aeEventLoop *base,
							long long id,
							void *clientData) {
	struct session *session = (struct session *)clientData;
	sessionConnect(session);
	return AE_NOMORE;
}

static void onReConnect(struct session *session, 
						struct tcpClient *cli) {

	aeCreateTimeEvent(session->eventloop, 
			cli->retryDelayMs, 
			_startConnect, 
			session, 
			NULL);

	cli->retryDelayMs = 
		cli->retryDelayMs < kMaxRetryDelayMs ? 
		cli->retryDelayMs * 2 : kMaxRetryDelayMs;
}


static void reConnect(struct session *session, struct tcpClient *cli) {
	if (isReConnect(cli)) {
		onReConnect(session, cli); 
	} else {
		// close
		//
		zSendx(NULL, 
			0,
			session->from,
			0,
			((MSG_TYPE_SOCK_CLOSE & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
			NULL, 
			0,
			buildSessionUd(session));
		// free ud
		zfree(cli);
	}
}

static void onConnection(struct session *session, void *ud) {

	zSendx(NULL, 
		0,
		session->from,
		0,
		((MSG_TYPE_SOCK_OPEN & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
		NULL, 
		0,
		buildSessionUd(session));
}

static void onClose(struct session *session, void *ud) {
	reConnect(session, (struct tcpClient*)ud);
}


static void onMessage(struct session *session, void *data, size_t sz, void *ud) {
	zSendx(NULL, 
		0,
		session->from,
		0,
		((MSG_TYPE_SOCK_DATA & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
		data, 
		sz,
		buildSessionUd(session));
}


struct netCallback tcpClientCallback = {
	onConnection,//connectionCallback onConnection;
	onClose,//closeCallback onClose;
	NULL,//writeCompleteCallback onWriteComplete;
	NULL,//hightWaterMarkCallback onHightWaterMark;
	NULL,//messageCallback onMessage;
	onMessage//protoMessageCallback onProtoMessage;
};


