#include "tcp_server.h" 

#include "callback.h"
#include "gen_tcp.h"
#include "zmalloc.h"
#include "z.h"

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
	zSendx(NULL, 
			0,
			session->from,
			0,
			((MSG_TYPE_SOCK_CLOSE & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
			NULL, 
			0,
			buildSessionUd(session));
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

struct netCallback tcpServerCallback = {
	onConnection,//connectionCallback onConnection;
	onClose,//closeCallback onClose;
	NULL,//writeCompleteCallback onWriteComplete;
	NULL,//hightWaterMarkCallback onHightWaterMark;
	NULL,//messageCallback onMessage;
	onMessage//protoMessageCallback onProtoMessage;
};


