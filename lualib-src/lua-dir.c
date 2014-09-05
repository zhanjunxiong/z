
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

#include <dirent.h>
#include <errno.h>
#include <string.h>

static const char* tname="z.dir";
static int dir_iter(lua_State* L) {
	DIR* d = *(DIR**)lua_touserdata(L, lua_upvalueindex(1));

	struct dirent* entry;
	if ((entry = readdir(d)) != NULL) {
		lua_pushstring(L, entry->d_name);
		if (entry->d_type == DT_DIR) {
			lua_pushstring(L, "dir");
		} else {
			lua_pushstring(L, "file");
		}
		return 2;
	}
	return 0;
}

static int l_dir(lua_State* L) {
	const char* path = luaL_checkstring(L, 1);

	DIR **d = (DIR**)lua_newuserdata(L, sizeof(DIR*));

	luaL_getmetatable(L, tname);
	lua_setmetatable(L, -2);

	*d = opendir(path);
	if (*d == NULL) {
		luaL_error(L, "cannot open %s: %s", path, strerror(errno));
	}

	lua_pushcclosure(L, dir_iter, 1);
	return 1;
}

static int dir_gc(lua_State* L) {
	DIR* d= *(DIR**)lua_touserdata(L, 1);
	if (d) closedir(d);
	return 0;
}

static const struct luaL_Reg lib[] = {
	{"open", l_dir},
	{NULL, NULL},
};

static const struct luaL_Reg libm[] = {
	{"__gc", dir_gc},
	{NULL, NULL},
};

extern "C" int luaopen_dir(lua_State*L) {
	luaL_checkversion(L);

	luaL_newmetatable(L, tname);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	luaL_setfuncs(L, libm, 0);

	luaL_newlib(L, lib);
	return 1;
}


