#include "session.h"
#include "buffer.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static int _write(lua_State*L) {
	struct session* session = (struct session*)lua_touserdata(L, 1);

	int type = lua_type(L, 2);
	if (type == LUA_TSTRING) {
		size_t len;
		const char* str = luaL_checklstring(L, 2, &len);
		sessionWrite(session, str, len);	
	} else if (type == LUA_TLIGHTUSERDATA) {
		const char* str = (const char*)lua_touserdata(L, 2);
		size_t len = luaL_checkinteger(L, 3);
		sessionWrite(session, str,len);
	}
	return 0;
}

static int _read(lua_State*L) {
	struct session* session = (struct session*)lua_touserdata(L, 1);
	struct buffer* buf = session->readBuffer;
	size_t len = bufferReadableBytes(buf);
	lua_pushlstring(L, bufferReadIndex(buf), len);
	bufferMoveReadIndex(buf, len);
	return 1;
}

static int _forceClose(lua_State*L) {
	struct session* session = (struct session*)lua_touserdata(L, 1);
	sessionForceClose(session);
	return 0;
}


static int _getId(lua_State*L) {
	struct session* session = (struct session*)lua_touserdata(L, 1);
	lua_pushnumber(L, session->id);
	return 1;
}

static const luaL_Reg lib[] = {
	{"write", _write},
	{"read", _read},
	{"forceClose", _forceClose},
	{"getId", _getId},
	{NULL, NULL},
};

static const luaL_Reg libm[] = {
	{NULL, NULL},
};


extern "C" int luaopen_session(lua_State *L) {
	/*
	luaL_newmetatable(L, tname);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, libm, 0);
	*/
	luaL_newlib(L, lib);
	return 1;
}

