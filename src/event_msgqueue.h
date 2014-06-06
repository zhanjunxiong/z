
#ifndef _EVENT_MSGQUEUE_H_
#define _EVENT_MSGQUEUE_H_

#include <stdint.h>

struct aeEventLoop;
struct em;
typedef void(*eventCallback)(void*, void*);

struct em* emCreate(struct aeEventLoop* base, 
		unsigned int, 
		eventCallback, 
		void *);
void emRelease(struct em*);

int emPush(struct em*, void*);
uint32_t emLength(struct em*);

#endif


