#ifndef SERVICE_ZLUA_H_
#define SERVICE_ZLUA_H_

#include "z.h"

struct zlua {
	struct lua_State* L;
	struct context* ctx;
};

void zReportLuaCallError(struct lua_State* L, int from, int err);

#endif


