#include "protocol.h"

#include "buffer.h"
#include "callback.h"
#include "session.h"
#include "tcp_client.h"
#include "tcp_server.h"

#include <string.h>
#include <stdio.h>

static int ON_PROTO_MESSAGE(struct session* session, 
							const char* protoName, 
							const char* cmd, 
							uint32_t seq,
							void* body, 
							size_t len) {

	session->onNetCallback.onProtoMessage(session, protoName, cmd, seq, body, len); 
	return 0;
}

struct proto {
	const char* name;
	messageCallback cb;	
};

//struct ProtobufTransportFormat __attribute__ ((__packed__))
//{
//	int32 	len;
//	int32 	seq;
//	int32	nameLen;
//	char	typeName[nameLen];
//	char	protobufData[len-nameLen-2*sizeof(int32)];
//}
void pbMessageCallback(struct session* session, struct buffer* buf) {
	static uint32_t kHeaderLen = sizeof(int32_t);
	static uint32_t kMinMessageLen = 0;
	static uint32_t kMaxMessageLen = 10 * 1024 * 1024;
	while(bufferReadableBytes(buf) >= kMinMessageLen + kHeaderLen) {
		uint32_t len = bufferPeekInt32(buf);
		//fprintf(stderr, "len: %d, bytes: %d\n", len, bufferReadableBytes(buf));
		if (len > kMaxMessageLen || len < kMinMessageLen) {
			// 非法包 
			// 断开这个session
			sessionForceClose(session);
			break;
		} else if (bufferReadableBytes(buf) >= (len + kHeaderLen)) {
			// head
			len = bufferReadInt32(buf);
			// seq
			uint32_t seq = bufferReadInt32(buf);
			// nameLen
			int32_t nameLen = bufferReadInt32(buf);
			// name
			char name[nameLen+1];
			memcpy(name, bufferPeek(buf), nameLen);
			name[nameLen] = '\0';
			bufferMoveReadIndex(buf, nameLen);
			// body
			char* body = bufferPeek(buf);
			int32_t pbLen = len - 2*sizeof(int32_t) - nameLen;
			ON_PROTO_MESSAGE(session, "pb", name, seq, body, pbLen);
			bufferMoveReadIndex(buf, pbLen);
		} else {
			break;
		}
	}
}

//struct HeadTransportFormat __attribute__ ((__packed__))
//{
//	int32 	len;
//	char	data[len];
//}
void headMessageCallback(struct session* session, struct buffer* buf) {
	static uint32_t kHeaderLen = sizeof(int32_t);
	static uint32_t kMinMessageLen = 0;
	static uint32_t kMaxMessageLen = 10 * 1024 * 1024;
	while(bufferReadableBytes(buf) >= kMinMessageLen + kHeaderLen) {
		uint32_t len = bufferPeekInt32(buf);
		if (len > kMaxMessageLen || len < kMinMessageLen) {
			/// 非法包 
			// 断开这个session
			sessionForceClose(session);
			break;
		} else if (bufferReadableBytes(buf) >= (len + kHeaderLen)) {
			ON_PROTO_MESSAGE(session, "head", "", 0, bufferPeek(buf) + kHeaderLen, len);
			bufferMoveReadIndex(buf, len + kHeaderLen);
		} else {
			break;
		}
	}
}


void textMessageCallback(struct session* session, struct buffer* buf) {
	int len = bufferReadableBytes(buf);
	ON_PROTO_MESSAGE(session, "text", "", 0, bufferPeek(buf), len);
	bufferMoveReadIndex(buf, len);
}

struct proto protoCallback[] = {
	{"pb", pbMessageCallback},
	{"head", headMessageCallback},
	{"text", textMessageCallback},
	{NULL, NULL},
};

void defaultMessageCallback(struct session* session, struct buffer* buf) {
	struct proto* method = &protoCallback[0];
	while(method->name) {
		if (strcmp(method->name, session->protoName) == 0) {
			return method->cb(session, buf);
		}
		++method;
	}
	return;
}


