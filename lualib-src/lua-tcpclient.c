#include "tcp_client.h"
#include "session.h"
#include "service_zlua.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static const char* tname = "tcpclientlib";
static int _create(lua_State *L) {
	const char* name = luaL_checkstring(L, 1);
	const char* serverAddr = luaL_checkstring(L, 2);
	int serverPort = luaL_checkinteger(L, 3);
	struct tcpClient* client = tcpClientCreate(name, serverAddr, serverPort);
	lua_pushlightuserdata(L, client);
	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
	return 1;
}

/*
static int _release(lua_State *L) {
	struct tcpClient* client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	tcpClientRelease(client);
	return 0;
}
*/

static int _start(lua_State *L) {
	struct tcpClient* client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	int ret = tcpClientStart(client);
	lua_pushnumber(L, ret);
	return 1;
}

static void _onConnection(struct session* session) {
	lua_State* L = (lua_State*) (session->privateData);
	lua_rawgetp(L, LUA_REGISTRYINDEX, (const void*)_onConnection);

	lua_pushinteger(L, session->id);
	int err = lua_pcall(L, 1, 0, 1);
	zReportLuaCallError(L, "onClose", err);
}

static int _connectionCallback(lua_State *L) {
	struct tcpClient* client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onConnection);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpClientConnectionCallback(client, _onConnection);
	tcpClientPrivateData(client, gl);
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
	struct tcpClient* Client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onClose);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpClientCloseCallback(Client, _onClose);
	tcpClientPrivateData(Client, gl);
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
	struct tcpClient* Client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onWriteComplete);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpClientWriteCompleteCallback(Client, _onWriteComplete);
	tcpClientPrivateData(Client, gl);
	return 0;
}

static void _onHightWaterMark(struct session* session, size_t size) {
	lua_State* L = (lua_State*) session->privateData;
	lua_rawgetp(L, LUA_REGISTRYINDEX,(const void*)_onHightWaterMark);

	lua_pushinteger(L, session->id);
	lua_pushnumber(L, size);
	int err = lua_pcall(L, 2, 0, 1);
	zReportLuaCallError(L, "onHightWaterMark", err);
	return;
}


static int _hightWaterMarkCallback(lua_State *L) {
	struct tcpClient* Client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onHightWaterMark);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpClientHightWaterMarkCallback(Client, _onHightWaterMark);
	tcpClientPrivateData(Client, gl);
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
	struct tcpClient* Client = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onMessage);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpClientMessageCallback(Client, _onMessage);
	tcpClientPrivateData(Client, gl);
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
	struct tcpClient* srv = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	luaL_checktype(L, 2, LUA_TFUNCTION);
	
	lua_pushvalue(L, 2);
	lua_rawsetp(L, LUA_REGISTRYINDEX, (const void*)_onProtoMessage);

	lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_MAINTHREAD);
	lua_State* gl = lua_tothread(L, -1);
	
	tcpClientProtoMessageCallback(srv, _onProtoMessage);
	tcpClientPrivateData(srv, gl);
	return 0;
}

static int _setProto(lua_State *L) {
	struct tcpClient* srv = (struct tcpClient*)luaL_checkudata(L, 1, tname);
	const char* protoName = luaL_checkstring(L, 2);
	tcpClientSetProto(srv, protoName);	
	return 0;
}

static int _send(lua_State* L) {
	size_t sz;
	const char* name = luaL_checklstring(L, 1, &sz);
	int type = luaL_checkinteger(L, 2);
	void* data = NULL;
	int mtype = lua_type(L, 3);
	if (mtype == LUA_TSTRING) {
		data = (void*)lua_tolstring(L, 3, &sz);
	} else if (mtype == LUA_TLIGHTUSERDATA) {
		data = lua_touserdata(L, 3);
		sz = luaL_checkinteger(L, 4);
	} else {
		luaL_error(L, "redirect invalid param %s", lua_typename(L, mtype));
		return 0;
	}

	tcpClientSend(name, type, data, sz);
	return 0;
}

static const luaL_Reg lib[] = {
	{"create", _create},
	{"send", _send},
	{NULL, NULL},
};


static const luaL_Reg libm[] = {
	//{"__gc",_release},
	{"start", _start},
	{"connection_callback", _connectionCallback},
	{"close_callback", _closeCallback},
	{"writecomplete_callback", _writeCompleteCallback},
	{"hightwatermark_callback", _hightWaterMarkCallback},
	{"message_callback", _messageCallback},
	{"protomessage_callback", _protoMessageCallback},
	{"set_proto", _setProto},
	{NULL, NULL},
};


extern "C" int luaopen_tcpclient(lua_State *L) {
	luaL_newmetatable(L, tname);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");

	luaL_setfuncs(L, libm, 0);
	luaL_newlib(L, lib);
	return 1;
}

