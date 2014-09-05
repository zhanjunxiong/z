#include "proto.h"

#include "buffer.h"
#include "define.h"
#include "socket.h"
#include "z.h"

#include <assert.h>

typedef void(*messageCallback)(struct socket *, struct buffer *);

struct protoCB {
	messageCallback cb;	
};

static void protoMessageCallback(struct socket *socket,  
								void *data, 
								size_t sz) {
	zSendx(NULL, 
		0,
		socket->from,
		0,
		((MSG_TYPE_SOCK_DATA & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
		data, 
		sz,
		socket->id);
}

//struct HeadTransportFormat __attribute__ ((__packed__))
//{
//	int32 	len;
//	char	data[len];
//}
void headMessageCallback(struct socket *socket, struct buffer *buf) {
	static uint32_t kHeaderLen = sizeof(int32_t);
	static uint32_t kMinMessageLen = 0;
	static uint32_t kMaxMessageLen = 10 * 1024 * 1024;

	while(bufferReadableBytes(buf) >= kMinMessageLen + kHeaderLen) {
		uint32_t len = bufferPeekInt32(buf);
		if (len > kMaxMessageLen || len < kMinMessageLen) {
			/// 非法包 
			// 断开这个socket
			socketForceClose(socket);
			break;
		} else if (bufferReadableBytes(buf) >= (len + kHeaderLen)) {
			char *body = (char*)zmalloc(len+1);
			memcpy(body, bufferPeek(buf) + kHeaderLen, len);
			protoMessageCallback(socket, body, len);
			bufferMoveReadIndex(buf, len + kHeaderLen); 
		} else {
			break;
		}
	}
}

void httpMessageCallback(struct socket *socket, struct buffer *buf) {
	protoMessageCallback(socket,  bufferPeek(buf), bufferReadableBytes(buf));
	bufferReset(buf);
}

struct protoCB protocbs[] = {
	{NULL},
	{headMessageCallback},
	{httpMessageCallback},
};

void onMessageCallback(struct socket *socket, struct buffer *buf) {
	assert( (socket->protoType > 0) && 
			(socket->protoType < (int)(sizeof(protocbs)/sizeof(struct protoCB))) );

	struct protoCB* method = &protocbs[socket->protoType];
	if (method) {
		return method->cb(socket, buf);
	}
}


