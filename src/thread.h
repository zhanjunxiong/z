#ifndef THREAD_H_
#define THREAD_H_

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

struct thread;

typedef void*(*threadFunc)();

struct thread* threadCreate(threadFunc cb, const char* name);
void threadRelease(struct thread*);
bool threadStarted(struct thread*);
void threadStart(struct thread*);
void threadJoin(struct thread*);
pid_t threadTid(struct thread*);
const char* threadName(struct thread*);


#endif

