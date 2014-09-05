#ifndef CALLBACK_H_
#define CALLBACK_H_

#include "define.h"

struct session;
struct buffer;
typedef void(*connectionCallback)(struct session*, void*);
typedef void(*closeCallback)(struct session*, void*);
typedef void(*writeCompleteCallback)(struct session*, void*);
typedef void(*hightWaterMarkCallback)(struct session*, size_t, void*);
typedef void(*messageCallback)(struct session*, struct buffer*);
typedef void(*protoMessageCallback)(struct session*, void *data, size_t, void*);

struct netCallback {
	connectionCallback onConnection;
	closeCallback onClose;
	writeCompleteCallback onWriteComplete;
	hightWaterMarkCallback onHightWaterMark;
	messageCallback onMessage;
	protoMessageCallback onProtoMessage;
};

#endif


