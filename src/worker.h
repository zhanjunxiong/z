#ifndef WORKER_H_
#define	WORKER_H_

#define	IDLE_STATE 0
#define	WORK_STATE 1

#include <stdint.h>
#include <pthread.h>

struct contextQueue;
struct thread;
struct worker {
	struct contextQueue *queue;
	struct thread *thread;
	pthread_mutex_t lock;
	pthread_cond_t cond;
	pthread_t pid;
	int state;
	int id;

};

struct worker *workerCreate();
void workerRelease(struct worker*);

int workerStart(struct worker*);
void workerJoin(struct worker*);

void workerSleep(struct worker*);
void workerWakeUp(struct worker*);


#endif

