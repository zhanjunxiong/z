#include "env.h"

#include "define.h"
#include "log.h"
#include "z.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

extern "C"
{
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

struct env {
	pthread_mutex_t lock;
	lua_State* L;
};

static struct env* E = NULL;
const char* envGet(const char* key, const char* defaultValue) {
	pthread_mutex_lock(&E->lock);

	lua_State* L = E->L;
	lua_getglobal(L, key);
	const char* result = lua_tostring(L, -1);
	lua_pop(L, 1);
	pthread_mutex_unlock(&E->lock);

	if (result) {
		return result;
	}
	return defaultValue;
}

int envGetInt(const char* key, int defaultValue) {
	const char* value = envGet(key, NULL);
	if (!value) {
		return defaultValue;
	}
	return strtol(value, NULL, 10);
}

int envSet(const char* key, const char* value) {
	pthread_mutex_lock(&E->lock);

	lua_State* L = E->L;
	lua_getglobal(L, key);
	assert(lua_isnil(L, -1));
	lua_pop(L, 1);
	lua_pushstring(L, value);
	lua_setglobal(L, key);

	pthread_mutex_unlock(&E->lock);

	return 0;
}

static void l_message (const char *pname, const char *msg) {
	if (pname) {
	  	luai_writestringerror("%s: ", pname);
  	}
  	luai_writestringerror("%s\n", msg);
}

static const char *progname = Z_PROGNAME;
static int report (lua_State *L, int status) {
  if (status != LUA_OK && !lua_isnil(L, -1)) {
    const char *msg = lua_tostring(L, -1);
    if (msg == NULL) msg = "(error object is not a string)";
    l_message(progname, msg);
    lua_pop(L, 1);
    /* force a complete garbage collection in case of errors */
    lua_gc(L, LUA_GCCOLLECT, 0);
  }
  return status;
}

static int dofile (lua_State *L, const char *name) {
  int status = luaL_loadfile(L, name);
  if (status == LUA_OK) status = lua_pcall(L, 0, 0, 0);
  return report(L, status);
}

static int dostring (lua_State *L, const char *s, const char *name) {
  int status = luaL_loadbuffer(L, s, strlen(s), name);
  if (status == LUA_OK) status = lua_pcall(L, 0, 0, 0);
  return report(L, status);
}

static int handleInit(lua_State* L) {
	const char *name = "=" Z_INITVERSION;
	const char *init = getenv(name + 1);
	if (init == NULL) {
		name = "=" Z_INIT;
		init = getenv(name + 1);  /* try alternative name */
	}
	if (init == NULL) {
		init = Z_DEFINE_ENV;
	}
	if (init == NULL) {
		return LUA_OK;
	}
	else if (init[0] == '@') {
		return dofile(L, init+1);
	}
	else {
		return dostring(L, init, name);
	}
}

int envInit(const char *envFileName) {

	E = (struct env*)zcalloc(sizeof(*E));
	pthread_mutex_init(&E->lock, NULL);

	lua_State* L = E->L = luaL_newstate();

  	lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
	luaL_openlibs(L);
  	lua_gc(L, LUA_GCRESTART, 0);

	if (handleInit(L) == LUA_OK) {
		return 0;
	}
	return -1;
}

void envUninit() {
	lua_State* L = E->L;
	lua_getglobal(L, "uninit");
	int type = lua_type(L, -1);
	if (type == LUA_TFUNCTION) {
		int status = lua_pcall(L, 0, 0, 0);
		report(L, status);
	} else {
		lua_pop(L, 1);
	}

	pthread_mutex_destroy(&E->lock);
	lua_close(L);
	zfree(E);
}


