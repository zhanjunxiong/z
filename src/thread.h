#ifndef THREAD_H_
#define THREAD_H_

#include <pthread.h>

struct thread;

typedef void*(*threadFunc)(void*);

struct thread* threadCreate(threadFunc cb, const char* name);
void threadRelease(struct thread*);

void threadStart(struct thread*);
void threadJoin(struct thread*);

pid_t threadTid(struct thread*);
const char* threadName(struct thread*);


#endif

