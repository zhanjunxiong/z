
#include "lua-seri.h"
#include "service_zlua.h"
#include "z.h"

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdint.h>
#include <stdlib.h>

/*
#include <pthread.h>

static void stack_dump(lua_State *L, struct context *ctx, struct message *msg) {
	int i;
	int top = lua_gettop(L);
	fprintf(stderr, "S========= top(%d) pid(%02x) L(%x) id(%d), msg(%d)\n", 
					top, pthread_self(), L, ctx->id, msg->type);
	for (i=1; i <= top; i++) {
		int t = lua_type(L, i);
		switch(t) {
			case LUA_TSTRING:
				fprintf(stderr, "'%s'", lua_tostring(L, i));
				break;
			case LUA_TNUMBER:
				fprintf(stderr, "%g", lua_tonumber(L, i));
				break;
			case LUA_TBOOLEAN:
				fprintf(stderr, lua_toboolean(L, i) ? "true" : "false");
				break;
			default:
				fprintf(stderr, "type: %s", lua_typename(L, t));
				break;
		}
		fprintf(stderr, "\n");
	}
	fprintf(stderr, "E========= pid(%x) L(%x)\n", pthread_self(), L);
	
}
*/

static int traceback(lua_State *L) {
	size_t sz;
	const char *msg = lua_tolstring(L, 1, &sz);
	if (msg) {
		luaL_traceback(L, L, msg, 1);
	} else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

static int _cb(struct context* ctx, 
				void* ud, 
				struct message* msg) {

	lua_State* L = (lua_State*)ud;
	int trace = 1;
	int r;
	int top = lua_gettop(L);
	//stack_dump(L, ctx, msg);
	if (top == 0) {
		lua_pushcfunction(L, traceback);
		lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_cb);
	} else {
		assert(top == 2);
	}
	lua_pushvalue(L,2);

	int proto_type = msg->type & 0x0000FFFF;
	lua_pushinteger(L, proto_type);

	lua_pushinteger(L, msg->from);
	lua_pushinteger(L, msg->session);

	lua_pushlightuserdata(L, msg->data);
	lua_pushinteger(L, msg->sz);


	if (proto_type != MSG_TYPE_PROTO_SOCKET) {
		r = lua_pcall(L, 5, 0 , trace);
	} else {
		lua_pushinteger(L, ((msg->type >> 16) & 0xFFFF));
		lua_pushinteger(L, msg->ud);

		r = lua_pcall(L, 7, 0 , trace);
	}

	if (r == LUA_OK) {
		return 0;
	}

	zReportLuaCallError(L, ctx->id, r);
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
		lua_pushlstring(L, 
				result, 
				strlen(result));
		return 1;
	}
	return 0;
}

static int _send(lua_State* L) {
	struct context* ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	int to = 0;
	int mtype = lua_type(L, 1);
	if (mtype == LUA_TNUMBER) {
		to = luaL_checkinteger(L, 1);
	} else if (mtype == LUA_TSTRING) {
		size_t len;
		const char *name = lua_tolstring(L, 1, &len);
		to = nameMgrGetId(name);
	} else {
		luaL_error(L, "send invalid param %s", lua_typename(L, mtype));
		return 0;
	}
	
	int session = 0;
	int type = luaL_checkinteger(L, 3);
	if (lua_isnil(L, 2)) {
		type |= MSG_TYPE_NEWSESSION;
	} else {
		session = luaL_checkinteger(L, 2);
	}
	
	mtype = lua_type(L, 4);
	void* data = NULL;
	size_t sz = 0;
	if (mtype == LUA_TSTRING) {
		const char *tmp = lua_tolstring(L, 4, &sz);
		data = zmalloc(sz + 1);	
		memcpy(data, tmp, sz);
	} else if (mtype == LUA_TLIGHTUSERDATA) {
		data = lua_touserdata(L, 4);
		sz = luaL_checkinteger(L, 5);
	} else if (mtype == LUA_TNIL) {

	} else {
		luaL_error(L, "send invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	session = zSend(ctx, 0, to, session, type, data, sz);
	if (session < 0) {
		return 0;
	}
	lua_pushinteger(L, session);
	return 1;
}

static int _redirect(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	int from = 0;
	int mtype = lua_type(L, 1);
	if (mtype == LUA_TSTRING) {
		size_t len;
		const char *name = lua_tolstring(L, 1, &len);
		from = nameMgrGetId(name);
	} else if (mtype == LUA_TNUMBER) {
		from = luaL_checkinteger(L, 1);
	} else {
		luaL_error(L, "send invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	int to = 0;
	mtype = lua_type(L, 2);
	if (mtype == LUA_TSTRING) {
		size_t len;
		const char *name = lua_tolstring(L, 2, &len);
		to = nameMgrGetId(name);
	} else if (mtype == LUA_TNUMBER) {
		to = luaL_checkinteger(L, 2);
	} else {
		luaL_error(L, "send invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	uint32_t session = luaL_checkinteger(L, 3);
	uint32_t type = luaL_checkinteger(L, 4);

	mtype = lua_type(L, 5);
	void* data = NULL;
	size_t sz = 0;
	if (mtype == LUA_TSTRING) {
		const char *tmp = lua_tolstring(L, 5, &sz);
		data = zcalloc(sz + 1);	
		memcpy(data, tmp, sz);
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
	char *buffer = (char*)lua_touserdata(L, 1);
	int len = luaL_checknumber(L, 2);

	int32_t seq = 0;
	memcpy(&seq, buffer, sizeof(int32_t));

	int32_t nameLen = 0;
	memcpy(&nameLen, buffer + sizeof(int32_t), sizeof(int32_t));

	char name[nameLen + 1];
	memcpy(name, buffer + 2 * sizeof(int32_t), nameLen);

	int32_t headLen = 2 * sizeof(int32_t) + nameLen;
	int32_t bodyLen = len - headLen;

	if (bodyLen < 0) {
		lua_pushboolean(L, true);
		lua_pushstring(L, "body len error");
		return 2;
	}
	
	void *body = buffer + headLen;

	lua_pushboolean(L, true);
	lua_pushinteger(L, seq);
	lua_pushlstring(L, name, nameLen);
	lua_pushlightuserdata(L, body);
	lua_pushinteger(L, bodyLen);

	return 5;
}

static int _netpack(lua_State *L) {
	int seq = luaL_checkinteger(L, 1);
	size_t nameLen;
	const char *name = luaL_checklstring(L, 2, &nameLen);

	void *buffer = lua_touserdata(L, 3);
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

static int _sockettostring(lua_State *L) {
	if (lua_isnoneornil(L,1)) {
		return 0;
	}
	char* buffer = (char*)lua_touserdata(L,1);
	if (buffer == NULL) {
		return luaL_error(L, "deserialize null pointer");
	}

	int sz = luaL_checkinteger(L, 2);
	lua_pushlstring(L, buffer, sz);

	// free buffer
	// zfree(buffer);
	return 1;
}

static int _socketunpack(lua_State *L) {
	if (lua_isnoneornil(L,1)) {
		return 0;
	}
	void* buffer = lua_touserdata(L,1);
	if (buffer == NULL) {
		return luaL_error(L, "deserialize null pointer");
	}

	struct socketMessage* msg = (struct socketMessage*)buffer;	
	int ret = 3;
	lua_pushinteger(L, msg->ud);
	lua_pushinteger(L, msg->type);
	lua_pushinteger(L, msg->id);
	if (msg->data != NULL) {
		lua_pushlightuserdata(L, msg->data);
		lua_pushinteger(L, msg->sz);
		ret = 5;
	}

	// free buff
	//zfree(buffer);

	return ret;
}

static int _socketlisten(lua_State *L) {
	struct context *ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	size_t len;
	int protoType = luaL_checkinteger(L, 1);
	const char *listenAddr = luaL_checklstring(L, 2, &len);
	int listenPort = luaL_checkinteger(L, 3);

	int ret = tcpSocketListen(ctx, protoType, listenAddr, listenPort);
	lua_pushinteger(L, ret);
	return 1;
}
	
static int _socketconnect(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	size_t len;
	int protoType = luaL_checkinteger(L, 1);
	const char *addr = luaL_checklstring(L, 2, &len);
	int port = luaL_checkinteger(L, 3);

	int ret = tcpSocketConnect(ctx, 
								protoType, 
								addr, 
								port);
	lua_pushinteger(L, ret);
	return 1;
}

static int _socketasyncconnect(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	size_t len;
	int protoType = luaL_checkinteger(L, 1);
	const char *addr = luaL_checklstring(L, 2, &len);
	int port = luaL_checkinteger(L, 3);

	int ret = tcpSocketAsyncConnect(ctx, 
								protoType, 
								addr, 
								port);
	lua_pushinteger(L, ret);
	return 1;
}


static int _socketstart(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	int id = luaL_checkinteger(L, 1);
	int ret = tcpSocketStart(ctx, 
								id);
	lua_pushinteger(L, ret);
	return 1;
}


static int _socketSend(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	int idnum = 1;
	int mtype = lua_type(L, 1);
	if (mtype == LUA_TNUMBER) {
	} else if (mtype == LUA_TTABLE) {
		idnum = luaL_len(L, 1);
	} else {
		luaL_error(L, "send invalid param %s", lua_typename(L, mtype));
		return 0;
	}
	if (idnum > 500) {
		luaL_error(L, "send num too larger %d", idnum);
		return 0;
	}
	int ids[idnum];
	if (mtype == LUA_TTABLE) {
		int i;
		for (i = 1; i <= idnum; i++) {
			lua_rawgeti(L, 1, i);
			ids[i - 1] = lua_tointeger(L, -1);
			lua_pop(L, 1);
		}
	} else {
		ids[0] = lua_tointeger(L, 1);
	}

	mtype = lua_type(L, 2);
	void* data = NULL;
	size_t sz = 0;
	if (mtype == LUA_TSTRING) {
		const char*tmp = lua_tolstring(L, 2, &sz);
		data = zmalloc(sz + 1);
		memcpy(data, tmp, sz);
	} else if (mtype == LUA_TLIGHTUSERDATA) {
		data = lua_touserdata(L, 2);
		sz = luaL_checkinteger(L, 3);
	} else {
		luaL_error(L, "send invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	uint32_t ret = tcpSocketSend(ctx, ids, idnum, data, sz);
	lua_pushinteger(L, ret);
	return 1;
}

static int _closeSocket(lua_State *L) {
	struct context * ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	int id = luaL_checkinteger(L, 1);
	int ret = tcpSocketClose(ctx, id);
	lua_pushinteger(L, ret);
	return 1;
}

static int _regist(lua_State *L) {
	struct context *ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	
	int id = ctx->id;
	size_t sz;
	const char *name = lua_tolstring(L, 1, &sz);
	int mtype = lua_type(L, 2);
	if (mtype == LUA_TNUMBER) {
		id = luaL_checkinteger(L, 2);
	}else if (mtype == LUA_TNIL) {
	} else {
		luaL_error(L, "regist invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	int ret = nameMgrRegist(name, id);
	lua_pushinteger(L, ret);
	return 1;
}


static int _unregist(lua_State *L) {
	//struct context *ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	size_t sz;
	const char *name = lua_tolstring(L, 1, &sz);

	int ret = nameMgrUnRegist(name);
	lua_pushinteger(L, ret);
	return 1;
}

static int _name2id(lua_State *L) {
	//struct context *ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));

	size_t sz;
	const char *name = lua_tolstring(L, 1, &sz);

	int ret = nameMgrGetId(name);
	if (ret > 0) {
		lua_pushinteger(L, ret);
		return 1;
	}
	return 0;
}

static int _self(lua_State *L) {
	struct context *ctx = (struct context*)lua_touserdata(L, lua_upvalueindex(1));
	lua_pushinteger(L, ctx->id);
	return 1;
}

static const luaL_Reg lib[] = {
	{ "pack", _luaseri_pack },
	{ "unpack", _luaseri_unpack },

	{ "net_pack", _netpack },
	{ "net_unpack", _netunpack },

	{ "text_pack", _textpack },
	{ "text_unpack", _textunpack },


	{ "socket_listen", _socketlisten },
	{ "socket_connect", _socketconnect },
	{ "socket_asyncconnect", _socketasyncconnect },
	{ "socket_start", _socketstart },
	{ "socket_send", _socketSend },
	{ "socket_close", _closeSocket },
	{ "socket_unpack", _socketunpack },
	{ "socket_tostring", _sockettostring },

	{ "command" , _command },
	{ "callback", _callback },
	{ "redirect", _redirect },
	{ "send", _send },

	{ "regist", _regist },
	{ "unregist", _unregist },
	{ "name2id", _name2id },

	{ "self", _self },
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

