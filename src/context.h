#ifndef CONTEXT_H
#define CONTEXT_H

#include "messagequeue.h"

struct context;
typedef int(*contextCallback)(struct context *ctx, 
								void *ud, 
								struct message *msg);
	

struct context {
	uint32_t id;
	char result[32];
	char name[32];
	uint32_t ref;
	uint32_t sessionid;

	void *instance;
	struct module *mod;
	contextCallback cb;
	void *ud;
	
	struct messageQueue *queue;

	int exit;
	int state;

	int workerId;
	int inGlobal;
};

struct context *contextCreate(const char *modname, 
								const char *parm);
struct context *contextRelease(struct context *ctx);
void contextSetCallback(struct context *ctx, 
						contextCallback cb, 
						void *ud); 
uint32_t contextNewsession(struct context *ctx);
int contextDispatchMessage(struct context *ctx, struct thread *th);	

void contextRetain(struct context *ctx);

#endif


