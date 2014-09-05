#include "protocol.h"

#include "buffer.h"
#include "callback.h"
#include "gen_tcp.h"
#include "session.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

static int ON_PROTO_MESSAGE(struct session *session, 
							void *body, 
							size_t len) {
	struct netCallback *cb = session->cb;
	assert(cb != NULL);

	if (cb->onProtoMessage) {
		cb->onProtoMessage(session, body, len, session->ud);
	}

	return 0;
}

struct proto {
	messageCallback cb;	
};

//struct HeadTransportFormat __attribute__ ((__packed__))
//{
//	int32 	len;
//	char	data[len];
//}
void headMessageCallback(struct session* session, struct buffer* buf) {
	static uint32_t kHeaderLen = sizeof(int32_t);
	static uint32_t kMinMessageLen = 0;
	static uint32_t kMaxMessageLen = 10 * 1024 * 1024;
	struct netCallback *cb = session->cb;
	assert(cb != NULL);

	while(bufferReadableBytes(buf) >= kMinMessageLen + kHeaderLen) {
		uint32_t len = bufferPeekInt32(buf);
		if (len > kMaxMessageLen || len < kMinMessageLen) {
			/// 非法包 
			// 断开这个session
			sessionForceClose(session);
			break;
		} else if (bufferReadableBytes(buf) >= (len + kHeaderLen)) {
			if (cb->onProtoMessage) {
				char *body = (char*)zmalloc(len+1);
				memcpy(body, bufferPeek(buf) + kHeaderLen, len);
				cb->onProtoMessage(session, 
									body, 
									len, 
									session->ud);
			} 
			bufferMoveReadIndex(buf, len + kHeaderLen); 
		} else {
			break;
		}
	}
}

void httpMessageCallback(struct session* session, struct buffer* buf) {
	ON_PROTO_MESSAGE(session, bufferPeek(buf), bufferReadableBytes(buf));
	sessionResetReadBuffer(session);
}

struct proto protos[] = {
	{NULL},
	{headMessageCallback},
	{httpMessageCallback},
};

void defaultMessageCallback(struct session* session, struct buffer* buf) {
	assert( (session->protoType > 0) && 
			(session->protoType < (int)(sizeof(protos)/sizeof(struct proto))) );

	struct proto* method = &protos[session->protoType];
	if (method) {
		return method->cb(session, buf);
	}
}

void defaultConnectionCallback(struct session *session) {
	struct netCallback *cb = session->cb;
	assert(cb != NULL);

	if (cb->onConnection) {
		cb->onConnection(session, session->ud);
	}
}

void defaultCloseCallback(struct session *session){
	struct netCallback *cb = session->cb;
	assert(cb != NULL);

	if (cb->onClose) {
		cb->onClose(session, session->ud);
	}
	removeSession(session->id);
}

void defaultWriteCompleteCallback(struct session *session) {
	struct netCallback *cb = session->cb;
	assert(cb != NULL);
}

void defaultAcceptCallback(struct session *session, struct session *newSession) {
}


