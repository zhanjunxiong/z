#ifndef MESSAGE_H_
#define MESSAGE_H_

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct message {
	uint32_t from;
	uint32_t session;
	uint32_t type;
	uint32_t ud;
	size_t sz;
	void *data;
};

#endif

