#ifndef CONTEXT_H
#define CONTEXT_H

#include "message.h"

struct context;
struct queue;

// @return 返回	小于等于0 	释放掉msg 
// 				大于 0 		不释放msg
typedef int(*contextCallback)(struct context* ctx, 
								void* ud, 
								struct message* msg);
	
struct context {
	// 上下文名字
	char name[CONTEXT_NAME_MAX_LEN];
	char result[32];
	void* instance;

	struct module* mod;

	int ref;

	contextCallback cb;
	void* ud;

	uint32_t sessionid;

	struct queue* queue;
	bool init;
	// ctx 状态
	// 0 空闲状态
	// 1 在调度中
	// 2 退出状态
	int state;
	// 退出状态
	// 1 退出
	int exit;
};

struct context* contextCreate(const char* name, const char* modname, const char* parm);
struct context* contextRelease(struct context*);

uint32_t contextNewsession(struct context*);
void contextSetCallback(struct context* ctx, contextCallback cb, void* ud); 

void contextGrab(struct context*);

int contextSend(struct context* ctx, 
				const char* from,
				const char* to,
				uint32_t session,
				uint32_t type,
				void* data, 
				size_t sz);

int contextDispatchMessage(struct context*);	


#endif


