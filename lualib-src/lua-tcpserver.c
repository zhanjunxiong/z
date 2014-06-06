
#include "tcp_server.h"
#include "service_zlua.h"
#include "session.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static const char* tname = "tcpserverlib";
static int _create(lua_State *L) {
	const char* srvName = luaL_checkstring(L, 1);
	const char* listenAddr = luaL_checkstring(L, 2);
	int listenPort = luaL_checkinteger(L, 3);
	struct tcpServer* srv = tcpServerCreate(srvName, listenAddr, listenPort);
	lua_pushlightuserdata(L, srv);
	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
	return 1;
}

/*
static int _release(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	tcpServerRelease(srv);
	return 0;
}
*/

static int _start(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	int ret = tcpServerStart(srv);
	lua_pushnumber(L, ret);
	return 1;
}

static int _stop(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	tcpServerStop(srv);
	return 0;
}


static void _onConnection(struct session* session) {
	lua_State* L = (lua_State*) (session->privateData);
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onConnection);

	lua_pushinteger(L, session->id);
	int err = lua_pcall(L, 1, 0, 1);

	zReportLuaCallError(L, "onConnection", err);
}

static int _connectionCallback(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onConnection);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpServerConnectionCallback(srv, _onConnection);
	tcpServerPrivateData(srv, gl);
	return 0;
}

static void _onClose(struct session* session) {
	lua_State* L = (lua_State*) session->privateData;
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onClose);

	lua_pushinteger(L, session->id);
	int err = lua_pcall(L, 1, 0, 1);

	zReportLuaCallError(L, "onClose", err);
}


static int _closeCallback(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onClose);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpServerCloseCallback(srv, _onClose);
	tcpServerPrivateData(srv, gl);
	return 0;
}

static void _onWriteComplete(struct session* session) {
	lua_State* L = (lua_State*) session->privateData;
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onWriteComplete);

	lua_pushinteger(L, session->id);
	int err = lua_pcall(L, 1, 0, 1);
	zReportLuaCallError(L, "onWriteComplete", err);
}

static int _writeCompleteCallback(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onWriteComplete);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpServerWriteCompleteCallback(srv, _onWriteComplete);
	tcpServerPrivateData(srv, gl);
	return 0;
}

static void _onHightWaterMark(struct session* session, size_t size) {
	lua_State* L = (lua_State*) session->privateData;
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onHightWaterMark);

	lua_pushinteger(L, session->id);
	lua_pushnumber(L, size);
	int err = lua_pcall(L, 2, 0, 1);
	zReportLuaCallError(L, "onHightWaterMark", err);
}

static int _hightWaterMarkCallback(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX,(const void*) _onHightWaterMark);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpServerHightWaterMarkCallback(srv, _onHightWaterMark);
	tcpServerPrivateData(srv, gl);
	return 0;
}

static void _onMessage(struct session* session, struct buffer* buf) {
	lua_State* L = (lua_State*) session->privateData;
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onMessage);

	lua_pushinteger(L, session->id);
	lua_pushlightuserdata(L, buf);
	int err = lua_pcall(L, 2, 0, 1);
	zReportLuaCallError(L, "onMessage", err);
}


static int _messageCallback(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onMessage);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpServerMessageCallback(srv, _onMessage);
	tcpServerPrivateData(srv, gl);
	return 0;
}

static int _onProtoMessage(struct session* session, 
							const char* protoName, 
							const char* cmd, 
							uint32_t seq,
							void* msg, 
							size_t len) {

	lua_State* L = (lua_State*) session->privateData;
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onProtoMessage);

	lua_pushinteger(L, session->id);
	lua_pushstring(L, protoName);
	lua_pushstring(L, cmd);
	lua_pushinteger(L, seq);
	lua_pushlightuserdata(L, msg);
	lua_pushnumber(L, len);
	int err = lua_pcall(L, 6, 0, 1);
	zReportLuaCallError(L, "onProtoMessage", err);
	return 0;
}


static int _protoMessageCallback(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onProtoMessage);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpServerProtoMessageCallback(srv, _onProtoMessage);
	tcpServerPrivateData(srv, gl);
	return 0;
}

static int _setProto(lua_State *L) {
	struct tcpServer* srv = (struct tcpServer*)luaL_checkudata(L, 1, tname);
	const char* protoName = luaL_checkstring(L, 2);
	tcpServerSetProto(srv, protoName);	
	return 0;
}

static int _send(lua_State* L) {
	size_t sz;
	const char* name = luaL_checklstring(L, 1, &sz);
	uint32_t id = luaL_checkinteger(L, 2);
	int type = luaL_checkinteger(L, 3);
	void* data = NULL;
	int mtype = lua_type(L, 4);
	if (mtype == LUA_TSTRING) {
		data = (void*)lua_tolstring(L, 4, &sz);
	} else if (mtype == LUA_TLIGHTUSERDATA) {
		data = lua_touserdata(L, 4);
		sz = luaL_checkinteger(L, 5);
	} else {
		luaL_error(L, "redirect invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	tcpServerSend(name, id, type, data, sz);
	return 0;
}

static const luaL_Reg lib[] = {
	{"create", _create},
	{"send", _send},
	{NULL, NULL},
};

static const luaL_Reg libm[] = {
	//	{"__gc",_release},
	{"start", _start},
	{"stop", _stop},
	{"connection_callback", _connectionCallback},
	{"close_callback", _closeCallback},
	{"writecomplete_callback", _writeCompleteCallback},
	{"hightwatermark_callback", _hightWaterMarkCallback},
	{"message_callback", _messageCallback},
	{"protomessage_callback", _protoMessageCallback},
	{"set_proto", _setProto},
	{NULL, NULL},
};


extern "C" int luaopen_tcpserver(lua_State *L) {
	luaL_checkversion(L);

	luaL_newmetatable(L, tname);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, libm, 0);

	luaL_newlib(L, lib);
	return 1;
}

