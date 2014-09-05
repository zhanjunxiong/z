#include "service_zlua.h"

#include "z.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

static int traceback(lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	if (msg) {
		luaL_traceback(L, L, msg, 1);
	} else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

void zReportLuaCallError(lua_State* L,int from, int err) {
	if (err == LUA_OK) {
		return;
	}

	switch (err) {
	case LUA_ERRRUN:
		fprintf(stderr, "[%d] lua do error : %s\n", from, lua_tostring(L,-1));
		break;
	case LUA_ERRMEM:
		fprintf(stderr, "[%d] lua memory error : %s\n", from, lua_tostring(L, -1));
		break;
	case LUA_ERRERR:
		fprintf(stderr, "[%d] lua message error : %s\n", from, lua_tostring(L, -1));
		break;
	case LUA_ERRGCMM:
		fprintf(stderr, "[%d] lua gc error : %s\n", from, lua_tostring(L, -1));
		break;
	};
	lua_pop(L,1);
}

static int _init(struct zlua *l, 
				struct context *ctx, 
				const char *data, 
				size_t sz) {
	
	lua_State *L = l->L;
	l->ctx = ctx;

	lua_gc(L, LUA_GCSTOP, 0);
	luaL_openlibs(L);
	lua_pushlightuserdata(L, l);
	lua_setfield(L, LUA_REGISTRYINDEX, "zlua");

	const char *path = envGet("lua_path", "./lualib/?.lua");
	lua_pushstring(L, path);
	lua_setglobal(L, "LUA_PATH");

	const char *cpath = envGet("lua_cpath", "./luaclib/?.so");
	lua_pushstring(L, cpath);
	lua_setglobal(L, "LUA_CPATH");

	const char *service = envGet("luaservice", "./service/?.lua");
	lua_pushstring(L, service);
	lua_setglobal(L, "LUA_SERVICE");

	const char *preload = envGet("preload", NULL);
	lua_pushstring(L, preload);
	lua_setglobal(L, "LUA_PRELOAD");

	lua_pushcfunction(L, traceback);
	assert(lua_gettop(L) == 1);

	const char *loader = envGet("lualoader", "./lualib/lualoader.lua");
	int r = luaL_loadfile(L, loader);
	if (r != LUA_OK) {
		fprintf(stderr, "Can't load %s:%s", loader, lua_tostring(L, -1));
		zReportLuaCallError(L, ctx->id, r);
		return 1;
	}
	lua_pushlstring(L, data, sz);
	r = lua_pcall(L, 1, 0, 1);
	if (r != LUA_OK) {
		fprintf(stderr, "lua loader error : %s", lua_tostring(L, -1));
		zReportLuaCallError(L, ctx->id, r);
		return 1;
	}
	lua_settop(L, 0);
	
	lua_gc(L, LUA_GCRESTART, 0);
	return 0;
}

static int _launch(struct context *ctx, 
					void *ud, 
					struct message *msg) {
	assert(msg->session == 0 && ((msg->type&0x0000FFFF) == 0));
	// 取消掉这个回调
	zSetCallback(ctx, NULL, NULL);

	struct zlua* l = (struct zlua*)ud;
	int ret = _init(l, ctx, (const char*)msg->data, msg->sz);
	if (ret != 0) {
		// 加载lua文件失败 退出ctx
		char id[32];
		snprintf(id, 32, "%d", ctx->id);
		zCommand(ctx, "EXIT", id);
	}
	return 0;
}

extern "C" struct zlua *zluaCreate() {
	struct zlua *l = (struct zlua*)zcalloc(sizeof(*l));
	l->L = lua_newstate(zlalloc, NULL);
	return l;
}


extern "C" int zluaInit(struct zlua *l, struct context *ctx, const char *args) {
	size_t len = strlen(args);

	char *data = (char*)zmalloc(len + 1);
	memcpy(data, args, len);

	zSetCallback(ctx, _launch, l);
	zSend(ctx, 0, ctx->id, 0, 0, data, len);
	return 0;
}

extern "C" void zluaRelease(struct zlua *l) {
	lua_close(l->L);
	zfree(l);
}

