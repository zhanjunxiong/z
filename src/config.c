#include "config.h"

#include "log.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

struct config {
	pthread_mutex_t lock;
	lua_State* L;
};

static struct config* C = NULL;
const char* configGet(const char* key, const char* defaultValue) {
	pthread_mutex_lock(&C->lock);

	lua_State* L = C->L;
	lua_getglobal(L, key);
	const char* result = lua_tostring(L, -1);
	lua_pop(L, 1);
	pthread_mutex_unlock(&C->lock);

	if (result) {
		return result;
	}
	return defaultValue;
}

int configGetInt(const char* key, int defaultValue) {
	const char* value = configGet(key, NULL);
	if (!value) {
		return defaultValue;
	}
	return strtol(value, NULL, 10);
}

int configSet(const char* key, const char* value) {
	pthread_mutex_lock(&C->lock);

	lua_State* L = C->L;
	lua_getglobal(L, key);
	assert(lua_isnil(L, -1));
	lua_pop(L, 1);
	lua_pushstring(L, value);
	lua_setglobal(L, key);

	pthread_mutex_unlock(&C->lock);

	return 0;
}

int configInit(const char* fileName) {
	const char* fullFileName = fileName;
	C = (struct config*)zcalloc(sizeof(*C));
	pthread_mutex_init(&C->lock, NULL);
	lua_State*L = C->L = luaL_newstate();
	int err = luaL_dofile(L, fullFileName);
	if (err) {
		fprintf(stderr, "filename(%s) des(%s)\n", fullFileName, lua_tostring(L, -1));
		return 1;
	} else {
		//fprintf(stderr, "load file '%s' success\n", fullFileName);
	}
	return 0;
}

void configUninit() {
	pthread_mutex_destroy(&C->lock);
	lua_close(C->L);
	zfree(C);
}


