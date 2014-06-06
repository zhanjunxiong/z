#ifndef SERVICE_ZLUA_H_
#define SERVICE_ZLUA_H_

struct zlua {
	struct lua_State* L;
	struct context* ctx;
};

void zReportLuaCallError(struct lua_State* L, const char* from, int err);

#endif


