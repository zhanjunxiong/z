#include "message.h"

#include "context.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

struct message* messageCreate(struct context* ctx, 
								const char* from,
								uint32_t session,
								uint32_t type,
								void* data,
								size_t sz) {
	struct message* msg = (struct message*)zcalloc(sizeof(*msg));
	if (msg == NULL) {
		return msg;
	}

	size_t len = strlen(from);
	assert(len < CONTEXT_NAME_MAX_LEN - 1);
	memcpy(&msg->from, from, len);
	if (type & MSG_TYPE_NEWSESSION) {
		msg->session = contextNewsession(ctx);
	} else {
		msg->session = session;
	}
	msg->sz = sz;
	msg->type = type;
	if (type & MSG_TYPE_COPYDATA){
		msg->data = zmalloc(sz + 1);
		memcpy(msg->data, data, sz);
	} else {
		msg->data = data;
	}
	return msg;
}

void messageRelease(struct message* msg) {
	if (msg->type & MSG_TYPE_COPYDATA) {
		if (msg->data) {
			zfree(msg->data);
		}
	}
	zfree(msg);
}

