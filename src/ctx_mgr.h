#ifndef CTX_MGR_H_
#define CTX_MGR_H_

#include "context.h"

struct context *ctxMgrCreateContext(const char *modname, 
									const char *parm);
void ctxMgrReleaseContext(struct context*);

struct context *ctxMgrGetContext(uint32_t id);
int ctxMgrGetContextNum();

int ctxMgrInit(int setsize);
void ctxMgrUninit();

void ctxMgrStop();

#endif


