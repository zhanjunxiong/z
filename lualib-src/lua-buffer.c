#include "buffer.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static const char* tname = "buffer";

static int _tostring(lua_State *L) {
	struct buffer* buf = (struct buffer*)lua_touserdata(L, 1);
	const char* s = bufferReadIndex(buf);	
	size_t l = bufferReadableBytes(buf);
	lua_pushlstring(L, s, l);
	return 1;
}

static int _size(lua_State *L) {
	struct buffer* buf = (struct buffer*)lua_touserdata(L, 1);
	size_t size = bufferGetSize(buf);
	lua_pushnumber(L, size);
	return 1;
}


static const luaL_Reg lib[] = {
	{"tostring", _tostring},
	{"size", _size},
	{NULL, NULL},
};

static const luaL_Reg libm[] = {
	{NULL, NULL},
};



extern "C" int luaopen_buffer(lua_State *L) {
	luaL_newmetatable(L, tname);
	luaL_setfuncs(L, libm, 0);
	luaL_newlib(L, lib);
	return 1;
}

