#include "thread.h"

#include "log.h"
#include "zmalloc.h"

#include <assert.h>
#include <pthread.h>

struct thread {
	bool started;
	pthread_t pthreadId;
	pid_t tid;
	threadFunc func;
	const char* name;
	void* ud;
};

struct thread* threadCreate(threadFunc func, const char* name) {
	struct thread* th = (struct thread*)zmalloc(sizeof(*th));
	if (th == NULL)
		return NULL;

	th->started = false;
	th->pthreadId = 0;
	th->tid = 0;
	th->func = func;
	th->name = zstrdup(name);
	return th;
}

void threadRelease(struct thread* th) {
	assert(th != NULL);
	if (th->name) {
		zfree((void*)th->name);
	}
	zfree(th);
}

#include <unistd.h>
#include <sys/syscall.h>

pid_t gettid() {
	return syscall(SYS_gettid);
}

void _runInThread(struct thread* th) {
	th->tid = gettid();
	logInfo("start %s(%ld) thread", th->name, th->tid);
	th->func(th);
	logInfo("end %s(%ld) thread", th->name, th->tid);
}

static void* _startThread(void* obj){
	struct thread* th = (struct thread*) obj;
	_runInThread(th);
	return NULL;
}

void threadStart(struct thread* th) {
	assert(th != NULL);
	assert(!th->started);
	th->started = true;
	pthread_create(&th->pthreadId, NULL, &_startThread, th);
}

void threadJoin(struct thread* th) {
	assert(th->started);
	pthread_join(th->pthreadId, NULL);
}

pid_t threadTid(struct thread* th) {
	return th->tid;
}

const char* threadName(struct thread* th) {
	return th->name;
}


