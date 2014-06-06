#ifndef CALLBACK_H_
#define CALLBACK_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

struct session;
struct buffer;
typedef void(*connectionCallback)(struct session*);
typedef void(*closeCallback)(struct session*);
typedef void(*writeCompleteCallback)(struct session*);
typedef void(*hightWaterMarkCallback)(struct session*, size_t);
typedef void(*messageCallback)(struct session*, struct buffer*);
typedef int(*protoMessageCallback)(struct session*, 
									const char* protoName, 
									const char* cmd, 
									uint32_t seq,
									void* msg, 
									size_t len);

struct netCallback {
	connectionCallback onConnection;
	closeCallback onClose;
	writeCompleteCallback onWriteComplete;
	hightWaterMarkCallback onHightWaterMark;
	messageCallback onMessage;
	protoMessageCallback onProtoMessage;
};

#endif


