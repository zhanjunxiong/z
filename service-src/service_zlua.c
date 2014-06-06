#include "service_zlua.h"

#include "z.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include <sys/time.h>

static void currentTime(struct timeval* tv) {
	gettimeofday(tv, NULL);
}

#define SECTOUSEC 1000000
static double diffTime(struct timeval* tv) {
	struct timeval end;
	currentTime(&end);
	int diffsec = end.tv_sec - tv->tv_sec;
	int diffusec = (end.tv_usec - tv->tv_usec);
	if (diffusec < 0) {
		--diffsec;
		diffusec += SECTOUSEC;
	}
	return (double)diffsec + (double)diffusec / SECTOUSEC;
}

static int _tryLoad(lua_State* L, const char* path, int pathlen, const char* name) {
	int namelen = strlen(name);
	char tmp[pathlen + namelen];
	int i;
	for (i=0;i<pathlen;i++) {
		if (path[i] == '?')
			break;
		tmp[i] = path[i];
	}
	if (path[i] == '?') {
		memcpy(tmp+i,name,namelen);
		memcpy(tmp+i+namelen,path+i+1,pathlen - i -1);
	} else {
		fprintf(stderr,"zlua : Invalid lua context path\n");
		exit(1);
	}
	tmp[namelen+pathlen-1] = '\0';
	int r = luaL_loadfile(L,tmp);
	if (r == LUA_OK) {
		int i;
		for (i=namelen+pathlen-2;i>=0;i--) {
			if (tmp[i] == '/') {
				lua_pushlstring(L,tmp,i+1);
				lua_setglobal(L,"SERVICE_PATH");
				break;
			}
		}
		if (i<0) {
			return 0;
		}
		lua_getglobal(L,"package");
		lua_getfield(L,-1,"path");
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		luaL_addlstring(&b, tmp, i+1);
		luaL_addstring(&b, "?.lua;");
		luaL_addvalue(&b);
		luaL_pushresult(&b);
		lua_setfield(L,-2,"path");

		lua_pop(L,1);
		return 0;
	} else if (r == LUA_ERRFILE) {
		lua_pop(L,1);
		return -1;
	}
	return 1;
}

static int _load(lua_State* L, char** filename) {
	const char* name = strsep(filename, " \r\n");
	const char* path = zCommand(NULL, "GETENV", "luaservice");
	while (path[0]) {
		int pathlen;
		const char * pathend = strchr(path,';');
		if (pathend) {
			pathlen = pathend - path;
		} else {
			pathlen = strlen(path);
		}
		int r = _tryLoad(L, path, pathlen, name);
		if (r >=0) {
			return r;
		}
		path+=pathlen;
		if (path[0]==';')
			++path;
	}
	return -1;
}

static int traceback(lua_State *L) {
	const char *msg = lua_tostring(L, 1);
	if (msg) {
		luaL_traceback(L, L, msg, 1);
	} else {
		lua_pushliteral(L, "(no error message)");
	}
	return 1;
}

void zReportLuaCallError(lua_State* L, const char* from, int err) {
	if (err == LUA_OK) {
		return;
	}

	switch (err) {
	case LUA_ERRRUN:
		fprintf(stderr, "[%s] lua do error : %s", from, lua_tostring(L,-1));
		break;
	case LUA_ERRMEM:
		fprintf(stderr, "[%s] lua memory error : %s", from, lua_tostring(L, -1));
		break;
	case LUA_ERRERR:
		fprintf(stderr, "[%s] lua message error : %s", from, lua_tostring(L, -1));
		break;
	case LUA_ERRGCMM:
		fprintf(stderr, "[%s] lua gc error : %s", from, lua_tostring(L, -1));
		break;
	};
	lua_pop(L,1);
}


static int _init(struct zlua* l, struct context* ctx, struct message* msg) {
	
	lua_State* L = l->L;
	l->ctx = ctx;

	lua_gc(L, LUA_GCSTOP, 0);
	luaL_openlibs(L);
	lua_pushlightuserdata(L, l);
	lua_setfield(L, LUA_REGISTRYINDEX, "zlua");

	// 设置lua path 和 lua cpath
	const char* path = zCommand(NULL, "GETENV", "zlua_path");	
	if (path != NULL) {
		lua_getglobal(L, "package");
		lua_getfield(L, -1, "path");
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		luaL_addstring(&b, path);
		luaL_addstring(&b, ";");
		luaL_addvalue(&b);
		luaL_pushresult(&b);
		lua_setfield(L,-2,"path");

		lua_pop(L, 1);
	}
	const char* cpath = zCommand(NULL, "GETENV", "zlua_cpath");	
	if (cpath != NULL) {
		lua_getglobal(L, "package");
		lua_getfield(L, -1, "cpath");
		luaL_Buffer b;
		luaL_buffinit(L, &b);
		luaL_addstring(&b, cpath);
		luaL_addstring(&b, ";");
		luaL_addvalue(&b);
		luaL_pushresult(&b);
		lua_setfield(L,-2,"cpath");

		lua_pop(L, 1);
	}


	lua_gc(L, LUA_GCRESTART, 0);

	size_t sz = msg->sz;
	char tmp[sz+1];
	char *parm = tmp;
	strncpy(parm, (const char*)msg->data, sz);
	parm[sz] = '\0';

	lua_pushcfunction(L, traceback);
	int traceback_index = lua_gettop(L);
	assert(traceback_index == 1);

	const char* filename = parm;
	int r = _load(L, &parm);
	if (r != 0) {
		if (r<0) {
			fprintf(stderr, "lua parser [%s] load error, errno: %d\n", filename, r);
		} else {
			fprintf(stderr, "lua parser [%s] error : %s\n", filename, lua_tostring(L,-1));
		}
		return 1;
	}
	int n=0;
	while(parm) {
		const char * arg = strsep(&parm, " \r\n");
		if (arg && arg[0]!='\0') {
			lua_pushstring(L, arg);
			++n;
		}
	}
	r = lua_pcall(L,n,0,traceback_index);
	if (r == LUA_OK) {
		r = lua_gc(L, LUA_GCCOLLECT, 0);
		if (r == LUA_OK) {
			return 0;
		}
	}
	zReportLuaCallError(L, ctx->name, r);
	return 1;
}

static int _launch(struct context* ctx, 
					void* ud, 
					struct message* msg) {
	assert(msg->session == 0 && ((msg->type&0x0000FFFF) == 0));
	// 取消掉这个回调
	zSetCallback(ctx, NULL, NULL);

	struct zlua* l = (struct zlua*)ud;
	struct timeval tv;
	currentTime(&tv);

	int ret = _init(l, ctx, msg);
	// not free msg->data
	//
	double t = diffTime(&tv);
	lua_pushnumber(l->L, t);

	char name[50];
	snprintf(name, 49, "%s_boottime", ctx->name);
	lua_setfield(l->L, LUA_REGISTRYINDEX, name);
	if (ret != 0) {
		// 加载lua文件失败 退出ctx
		zCommand(ctx, "EXIT", ctx->name);
	}
	return 0;
}

static void* lalloc(void* uid, void*ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		zfree(ptr);
		return NULL;
	} else {
		return zrealloc(ptr, nsize);
	}
}

extern "C" struct zlua* zluaCreate() {
	struct zlua* l = (struct zlua*)zcalloc(sizeof(*l));
	l->L = lua_newstate(lalloc, NULL);
	return l;
}


extern "C" int zluaInit(struct zlua* l, struct context* ctx, const char* args) {
	zSetCallback(ctx, _launch, l);
	// todo 要放在最后面
	// 加载lua文件	
	size_t len = strlen(args);
	char tmp[len + 1];
	memcpy(tmp, args, len);
	zSend(ctx, NULL, ctx->name, 0, MSG_TYPE_COPYDATA, tmp, len);
	return 0;
}

extern "C" void zluaRelease(struct zlua* l) {
	lua_close(l->L);
	zfree(l);
}

