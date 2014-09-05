#include "module.h"

#include "log.h"
#include "zmalloc.h"

#include <assert.h>
#include <string.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#define MAX_MODULE_TYPE 32

struct modules {
	int count;
	int lock;
	char* path;
	struct module m[MAX_MODULE_TYPE];
};

static struct modules* M = NULL;

static void* _tryOpen(struct modules* m, const char* name) {
	const char* l;
	const char* path = m->path;
	size_t path_size = strlen(path);
	size_t name_size = strlen(name);

	int sz = path_size + name_size;
	void* dl = NULL;
	char tmp[sz];
	do
	{
		memset(tmp, 0, sz);
		while(*path == ';') path++;
		if (*path == '\0')break;
		l = strchr(path, ';');
		if (l == NULL) l = path + strlen(path);
		int len = l - path;
		int i;
		for (i=0; path[i]!='?' && i < len; i++) {
			tmp[i] = path[i];
		}
		memcpy(tmp+i, name, name_size);
		if (path[i] == '?') {
			strncpy(tmp+i+name_size, path+i+1, len-i-1);
		} else {
			fprintf(stderr, "Invalid C context path\n");
			exit(1);
		}

		dl = dlopen(tmp, RTLD_NOW | RTLD_GLOBAL);
		if (dl == NULL) {
			fprintf(stderr, "try open %s, fail, resutl: %s\n", tmp, dlerror());
		}

		path = l;
	}while(dl == NULL);

	if (dl == NULL) {
		fprintf(stderr, "try open %s failed: %s\n", name, dlerror());
	} else {
		//fprintf(stderr, "try open %s success\n", name);
	}

	return dl;
}

static struct module* _query(const char* name) {
	int i;
	for (i=0; i<M->count; i++) {
		if (strcmp(M->m[i].name, name) == 0) {
			return &M->m[i];
		}
	}
	return NULL;
}

static int _openSym(struct module* mod){

	size_t name_size = strlen(mod->name);
	char tmp[name_size + 9];
	memcpy(tmp, mod->name, name_size);
	strcpy(tmp+name_size, "Create");
	mod->create = (dlCreate)dlsym(mod->module, tmp);

	strcpy(tmp+name_size, "Init");
	mod->init = (dlInit)dlsym(mod->module, tmp);

	strcpy(tmp+name_size, "Release");
	mod->release = (dlRelease)dlsym(mod->module, tmp);
	
	return mod->init == NULL;
}

struct module* moduleQuery(const char* name) {
	struct module* result = _query(name);
	if (result)
		return result;

	while(__sync_lock_test_and_set(&M->lock, 1)) {}

	result = _query(name);
	
	if (result == NULL && M->count < MAX_MODULE_TYPE) {
		int index = M->count;
		void* dl = _tryOpen(M, name);
		if (dl) {
			M->m[index].name = name;
			M->m[index].module = dl;

			if (_openSym(&M->m[index]) == 0){
				M->m[index].name = zstrdup(name);
				M->count++;
				result = &M->m[index];
			}
		}
	}

	__sync_lock_release(&M->lock);

	return result;
}

void moduleInsert(struct module* mod) {
	while(__sync_lock_test_and_set(&M->lock, 1)) {}

	struct module* m = _query(mod->name);
	assert(m == NULL && M->count < MAX_MODULE_TYPE);
	int index = M->count;
	M->m[index] = *mod;
	++M->count;
	__sync_lock_release(&M->lock);
}

void* moduleInstanceCreate(struct module* m) {
	if (m->create) {
		return m->create();
	} else {
		return (void*)(intptr_t)(~0);
	}
}

int moduleInstanceInit(struct module* m, void* inst, struct context* ctx, const char* parm) {
	return m->init(inst, ctx, parm);
}

void moduleInstanceRelease(struct module* m, void* inst) {
	if (m->release) {
		return m->release(inst);
	}
}

int moduleInit(const char* path) {
	struct modules* m = (struct modules*)zmalloc(sizeof(*m));
	m->count = 0;
	m->path = zstrdup(path);
	m->lock = 0;
	M = m;
	return 0;
}

void moduleUninit() {
	int i;
	for (i=0; i<M->count; i++) {
		struct module m = M->m[i];
		if (m.module) {
			//logInfo("close module name: %s", m.name);
			dlclose(m.module);
		}
		if (m.name) {
			zfree((void*)m.name);
		}
	}

	if (M->path) {
		zfree(M->path);
	}
	zfree(M);
}

