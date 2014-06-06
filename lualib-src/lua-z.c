
#include "lua-seri.h"
#include "service_zlua.h"
#include "z.h"

#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdint.h>
#include <stdlib.h>

static int _cb(struct context* ctx, 
				void* ud, 
				struct message* msg) {

	lua_State* L = (lua_State*)ud;
	int trace = 1;
	int r;
	int top = lua_gettop(L);
	if (top == 1) {
		lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_cb);
	} else {
		assert(top == 2);
		lua_pushvalue(L,2);
	}

	lua_pushinteger(L, msg->type);
	lua_pushstring(L, msg->from);
	lua_pushinteger(L, msg->session);

	lua_pushlightuserdata(L, msg->data);
	lua_pushinteger(L, msg->sz);

	r = lua_pcall(L, 5, 0 , trace);

	if (r == LUA_OK) {
		return 0;
	}

	zReportLuaCallError(L, ctx->name, r);
	return 0;
}

static int _callback(lua_State *L) {
	struct context* ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	luaL_checktype(L,1,LUA_TFUNCTION);
	lua_settop(L,1);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_cb);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gL = lua_tothread(L, -1);

	zSetCallback(ctx, _cb, gL);
	return 0;
}

static int _command(lua_State *L) {
	struct context* ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	const char* cmd = luaL_checkstring(L,1);
	const char* result;
	const char* parm = NULL;
	if (lua_gettop(L) == 2) {
		parm = luaL_checkstring(L,2);
	}

	result = zCommand(ctx, cmd, parm);
	if (result) {
		lua_pushstring(L, result);
		return 1;
	}
	return 0;
}

static int _send(lua_State* L) {
	struct context* ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	size_t len;
	const char* to = luaL_checklstring(L, 1, &len);

	int session = luaL_checkinteger(L, 2);
	int type = luaL_checkinteger(L, 3);
	
	int mtype = lua_type(L, 4);
	void* data = NULL;
	size_t sz = 0;
	if (mtype == LUA_TSTRING) {
		data = (void*)lua_tolstring(L, 4, &sz);
		// 打一个要malloc 的标记
		type = type | MSG_TYPE_COPYDATA;
	} else if (mtype == LUA_TLIGHTUSERDATA) {
		data = lua_touserdata(L, 4);
		sz = luaL_checkinteger(L, 5);
	} else {
		luaL_error(L, "redirect invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	session = zSend(ctx, NULL, to, session, type, data, sz);
	if (session < 0) {
		return 0;
	}
	lua_pushinteger(L, session);
	return 1;
}

static int _redirect(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	size_t len;
	const char* from = luaL_checklstring(L, 1, &len);
	const char* to = luaL_checklstring(L, 2, &len);

	uint32_t session = luaL_checkinteger(L, 3);
	uint32_t type = luaL_checkinteger(L, 4);

	int mtype = lua_type(L, 5);
	void* data = NULL;
	size_t sz = 0;
	if (mtype == LUA_TSTRING) {
		data = (void*)lua_tolstring(L, 5, &sz);
		// 打一个要malloc 的标记
		type = type | MSG_TYPE_COPYDATA;
	} else if (mtype == LUA_TLIGHTUSERDATA) {
		data = lua_touserdata(L, 5);
		sz = luaL_checkinteger(L, 6);
	} else {
		luaL_error(L, "redirect invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	session = zSend(ctx, from, to, session, type, data, sz);
	return 0;	
}

static int _netunpack(lua_State *L) {
	void* buffer = lua_touserdata(L, 2);
	zfree(buffer);
	return 0;
}

static int _netpack(lua_State *L) {
	size_t nameLen;
	const char* name = luaL_checklstring(L, 1, &nameLen);
	int seq = luaL_checkinteger(L, 2);
	void* buffer = lua_touserdata(L, 3);
	size_t bufferLen = luaL_checknumber(L, 4);

	int32_t len = nameLen + 2*sizeof(int32_t) + bufferLen;
	char* body = (char*)zmalloc(len + sizeof(int32_t));			
	memcpy(body, &len, sizeof(int32_t));
	memcpy(body + sizeof(int32_t), &seq, sizeof(int32_t));
	memcpy(body + 2*sizeof(int32_t), &nameLen, sizeof(int32_t));
	memcpy(body + 3*sizeof(int32_t), name, nameLen);
	memcpy(body + 3*sizeof(int32_t)+nameLen, buffer, bufferLen);
	lua_pushlightuserdata(L, body);
	lua_pushinteger(L, len+sizeof(int32_t));
	return 2;
}

static int _textpack(lua_State *L) {
	size_t sz;
	const char* data = luaL_checklstring(L, 1, &sz);
	char* text = (char*)zmalloc(sz + 1);
	memcpy(text, data, sz);
	lua_pushlightuserdata(L, text);
	lua_pushinteger(L, sz);
	return 2;
}

static int _textunpack(lua_State *L) {
	void* text = lua_touserdata(L, 2);
	zfree(text);
	return 0;
}

static const luaL_Reg lib[] = {
	{ "pack", _luaseri_pack },
	{ "unpack", _luaseri_unpack },

	{ "net_pack", _netpack },
	{ "net_unpack", _netunpack },

	{ "text_pack", _textpack },
	{ "text_unpack", _textunpack },

	{ "command" , _command },
	{ "callback", _callback },
	{ "redirect", _redirect },
	{ "send", _send },

	{ NULL, NULL },
};

extern "C" int luaopen_z_c(lua_State *L) {
	luaL_checkversion(L);
	
	lua_getfield(L, LUA_REGISTRYINDEX, "zlua");
	struct zlua* lua = (struct zlua*)lua_touserdata(L,-1);
	if (lua == NULL || lua->ctx == NULL) {
		return luaL_error(L, "Init zlua context first");
	}
	assert(lua->L == L);

	luaL_newlibtable(L, lib);
	lua_pushlightuserdata(L, lua->ctx);
	luaL_setfuncs(L, lib, 1);
	return 1;
}

