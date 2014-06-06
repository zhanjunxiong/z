
#ifndef MODULE_H_
#define MODULE_H_

struct context;

typedef void* (*dlCreate)();
typedef int (*dlInit)(void* init, struct context*, const char* parm);
typedef void (*dlRelease)(void* init);

struct module{
	const char* name;
	void* module;

	dlCreate create;
	dlInit init;
	dlRelease release;
};

void moduleInsert(struct module* module);
struct module* moduleQuery(const char* name);

void* moduleInstanceCreate(struct module*);
int moduleInstanceInit(struct module*, void* inst, struct context* ctx, const char* parm);
void moduleInstanceRelease(struct module*, void* inst);

int moduleInit(const char* path);
void moduleUninit();

#endif

