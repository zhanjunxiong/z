#ifndef CTX_MGR_H_
#define CTX_MGR_H_

struct context;
int ctxMgrAddContext(const char* name, struct context* ctx);
int ctxMgrRemoveContext(const char* name);
struct context* ctxMgrGetContext(const char* name);
int ctxMgrHasWork();

int ctxMgrInit();
void ctxMgrUninit();

#endif


