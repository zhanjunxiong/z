
#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// struct message
// message.type
// 高8位预留为系统内部消息
// 低8位预留位协议组列表 也就是说只支持 256(2^8) 中协议组
// 协议组
#define MSG_TYPE_PROTO_PB		0x00000001
#define MSG_TYPE_PROTO_LUA		0x00000002

// 系统内部消息相关
#define MSG_TYPE_NOTCOPYDATA	0x00010000
#define MSG_TYPE_COPYDATA		0x00020000
#define MSG_TYPE_RESPONSE		0x00040000
#define MSG_TYPE_NEWSESSION		0x00080000

#define CONTEXT_NAME_MAX_LEN 32

struct message {
	char from[CONTEXT_NAME_MAX_LEN];
	uint32_t session;
	void* data;
	uint32_t sz;
	// 消息类型
	uint32_t type;
};

struct context;
struct message* messageCreate(struct context* ctx, 
								const char* from,
								uint32_t session,
								uint32_t type,
								void* data,
								size_t sz);

void messageRelease(struct message* msg);

#endif

