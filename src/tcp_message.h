#ifndef TCP_MESSAGE_H_
#define TCP_MESSAGE_H_

#define TCP_MSG_TYPE_SEND	1
#define TCP_MSG_TYPE_CLOSE	2

#include <stdint.h>

struct tcpMessage {
	uint32_t id;
	int type;
	void* data;
	size_t sz;
};

#endif


