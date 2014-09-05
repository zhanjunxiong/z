
#include <mysql.h>
#include <my_sys.h>
#include <my_global.h>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <stdbool.h>

static const char* tname = "zmysql";

struct zmysql{
    MYSQL *conn;
};

static int _thread_init(lua_State *L){
	my_thread_init();
	return 0;
}

static int _thread_end(lua_State *L){
	my_thread_end(); 
	return 0;
}


static int _library_init(lua_State *L){
	mysql_library_init(0, NULL, NULL);
	return 0;
}

static int _library_end(lua_State *L){
	mysql_library_end(); 
	//my_end(0);
	return 0;
}

static int _test(lua_State* L) {
	return 0;
}

static int _connect(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)lua_newuserdata(L, sizeof(struct zmysql));


	MYSQL* conn = mysql_init(NULL);
	if (conn == NULL) {
		luaL_error(L, "zmysql mysql_init fail\n");
	}

	const char* host = luaL_checkstring(L, 1);
	const char* dbname = luaL_checkstring(L, 2);
	const char* user = luaL_checkstring(L, 3);
	const char* passwd = luaL_checkstring(L, 4);

	my_bool b = true;
	mysql_options(conn, MYSQL_OPT_RECONNECT, &b);
	if (mysql_real_connect(conn, 
				host, 
				user, 
				passwd, 
				dbname, 
				0, 
				NULL, 
				MYSQL_OPT_RECONNECT) == NULL) {

		mysql_close(conn);
		mysql_thread_end();
		luaL_error(L, "zmysql connect, host(%s), user(%s), passwd(%s), dbname(%s) fail, %s\n", 
					host,
					user,
					passwd,
					dbname,
					mysql_error(conn));

	}

	mysql_query(conn, "set names utf8");

	mysql->conn = conn;

	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);
	
	return 1;
}

static int _select(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)luaL_checkudata(L, 1, tname);

	MYSQL* conn = mysql->conn;
	if (conn == NULL) {
		luaL_error(L, "mysql disconnect");
	}

  	MYSQL_RES* result;

	const char* cmd = luaL_checkstring(L, 2);
	mysql_query(conn, cmd);
	result = mysql_store_result(conn);
	if (result == NULL) {
		luaL_error(L, "select fail %d %s command %s", 
					mysql_errno(conn),
					mysql_error(conn),
					cmd);
	}

    MYSQL_ROW row;
    MYSQL_FIELD* fields;
	int index = 0;

	lua_newtable(L);
	while(result != NULL) {
		fields = mysql_fetch_fields(result);
		if (fields == NULL) {
			mysql_free_result(result);
			luaL_error(L, "select fail fetch_fields cmd: %s", cmd);
			break;
		}

		int num_fields = mysql_num_fields(result);
		while((row = mysql_fetch_row(result))) {
			unsigned long* lengths = mysql_fetch_lengths(result);
			index++;
			lua_pushnumber(L, index);
			lua_newtable(L);
			int i;
			for (i = 0; i < num_fields; i++) {
				lua_pushstring(L, fields[i].name);
				if (row[i] != NULL) {
    				if(fields[i].type == FIELD_TYPE_STRING ||
                       fields[i].type == FIELD_TYPE_VAR_STRING ||
                       fields[i].type == FIELD_TYPE_TINY_BLOB ||
                       fields[i].type == FIELD_TYPE_MEDIUM_BLOB ||
                       fields[i].type == FIELD_TYPE_LONG_BLOB ||
                       fields[i].type == FIELD_TYPE_BLOB) {

                       lua_pushlstring(L, row[i], lengths[i]);
                   	}else if(fields[i].type == FIELD_TYPE_FLOAT ||
                            fields[i].type == FIELD_TYPE_DOUBLE){
                       lua_pushnumber(L, atof(row[i]));
                    } else {
                       lua_pushnumber(L, atoi(row[i]));
                   	}				
				} else {
                	lua_pushnil(L);
                }
                lua_settable(L, -3);
			}
            lua_settable(L, -3);
		}

		mysql_free_result(result);
		result = NULL;

     	//是否还有结果集
        if(mysql_next_result(conn) == 0){
        	result = mysql_store_result(conn);
        }
	}
    
    return 1;
}

static int _command(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)luaL_checkudata(L, 1, tname);

	MYSQL* conn = mysql->conn;
	if (conn == NULL) {
		luaL_error(L, "mysql disconnect");
	}

	const char* cmd = luaL_checkstring(L, 2);
	if ( mysql_query(conn, cmd) != 0 ) {
		lua_pushboolean(L, false);
		lua_pushstring(L, mysql_error(conn));
		return 2;
	}

	lua_pushboolean(L, true);
	return 1;
}

static int _affected_rows(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)luaL_checkudata(L, 1, tname);
    lua_pushnumber(L, mysql_affected_rows(mysql->conn));
    return 1;
}

static int _escape_string(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)luaL_checkudata(L, 1, tname);

	MYSQL* conn = mysql->conn;
	if (conn == NULL) {
		luaL_error(L, "mysql disconnect");
	}

	size_t strlen;
	const char* str= NULL;
	int type = lua_type(L, 2);
	if (type == LUA_TSTRING) {
		str = luaL_checklstring(L, 2, &strlen);
		if (strlen == 0) {
			lua_pushstring(L, "");
			return 1;
		}
	}else if (type == LUA_TLIGHTUSERDATA) {
		str = (const char*)lua_touserdata(L, 2);
		strlen = luaL_checknumber(L, 3);
	}

	char buf[strlen*2];
    int len = mysql_real_escape_string(conn, buf, str, strlen);
    lua_pushlstring(L, buf, len);
    return 1;
}

static int _close(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)luaL_checkudata(L, 1, tname);

    if(mysql->conn){
    	mysql_close(mysql->conn);
		mysql_thread_end();
        mysql->conn = NULL;
    }

	lua_pushboolean(L, true);
	return 1;
}

static int _ping(lua_State *L){
	struct zmysql* mysql = (struct zmysql*)luaL_checkudata(L, 1, tname);

	MYSQL* conn = mysql->conn;
	if (conn == NULL) {
		luaL_error(L, "mysql disconnect");
	}

	if(mysql_ping(conn)){
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, true);
	return 1;
}

static luaL_Reg lib[] = {
    {"connect", _connect},
	{"library_end", _library_end},
	{"library_init", _library_init},
	{"thread_init", _thread_init},
	{"thread_end", _thread_end},
	{"test", _test},
    {NULL, NULL}
};

static luaL_Reg libm[] = {
    {"select", _select},
    {"command", _command},
    {"affected_rows", _affected_rows},
    {"escape_string", _escape_string},
    {"close", _close},
    {"ping", _ping},
    {"__gc", _close},
    {NULL, NULL}
};

extern "C" int luaopen_zmysql(lua_State* L) {
	luaL_checkversion(L);
 
	luaL_newmetatable(L, tname);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, libm, 0);

	luaL_newlib(L, lib);
	return 1;
}


