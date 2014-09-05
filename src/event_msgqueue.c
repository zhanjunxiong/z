#include "event_msgqueue.h"

#include "ae.h"
#include "circqueue.h"
#include "zmalloc.h"

#include <assert.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>

#define LOCKFREE

#ifdef LOCKFREE
#include "arraylockfreequeue.h"
#endif

#ifdef LOCK
#endif


struct em{
   struct aeEventLoop *base;

   int pushFd;
   int popFd;

   eventCallback callback;

   void *cbarg;

#ifdef LOCK
   struct circQueue *queue;
   pthread_mutex_t lock;
   int unlockBetweenCallbacks;
#endif

   volatile int isExit;
   exitEventCallback exitCallback;

#ifdef LOCKFREE
   struct arrayLockFreeQueue *queue;
#endif

}; 

static void popMsgqueue(aeEventLoop* el, int fd, void *arg, int mask) {
   struct em *msgq = (struct em*)arg;
   char buf[64];
   recv(fd, buf, sizeof(buf),0);

#ifdef LOCK
   pthread_mutex_lock(&msgq->lock);
   while(!circQueueIsEmpty(msgq->queue)) {
      void *qdata = NULL;

      qdata = circQueuePopHead(msgq->queue);

      if (msgq->unlockBetweenCallbacks) {
         pthread_mutex_unlock(&msgq->lock);
      }
		
	  msgq->callback(qdata, msgq->cbarg);
 	 
      if (msgq->unlockBetweenCallbacks) {
         pthread_mutex_lock(&msgq->lock);
      }

   }
   pthread_mutex_unlock(&msgq->lock);
#endif

#ifdef LOCKFREE

   //fprintf(stderr, "len: %d\n", emLength(msgq));
   do {
		void *qdata = NULL;
		qdata = arrayLockFreeQueuePop(msgq->queue);
		if (qdata == NULL) {
			break;
		}

	  	msgq->callback(qdata, msgq->cbarg);
   } while(true);
#endif

   if (msgq->isExit) {
	   msgq->exitCallback();
   }
}


struct em* emCreate(struct aeEventLoop* base, 
			unsigned int maxSize, 
			eventCallback callback,
			void* cbarg) {
	assert(base != NULL);
	
	struct em* msgq;
	if (!(msgq = (struct em*)zmalloc(sizeof(*msgq)))) {
      	return NULL;
   	}

	int fds[2];

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
		return NULL;
	}

#ifdef LOCK
	struct circQueue* cq;
	if (!(cq = circQueueCreate(maxSize)))
		return NULL;

	pthread_mutex_init(&msgq->lock, NULL);
	msgq->unlockBetweenCallbacks = 1;
	msgq->queue = cq;
#endif

#ifdef LOCKFREE
	msgq->queue = arrayLockFreeQueueCreate(maxSize);
#endif


	msgq->pushFd = fds[0];
	msgq->popFd = fds[1];

	msgq->callback = callback;
	msgq->cbarg = cbarg;

	msgq->base = base;
	msgq->isExit = 0;
    msgq->exitCallback = NULL;

	aeCreateFileEvent(base, msgq->popFd, AE_READABLE, popMsgqueue, msgq);
	return(msgq);
}

void emExit(struct em* msgq) {
	msgq->isExit = 1;
}

void emSetExitCallback(struct em* msgq, exitEventCallback cb) {
	msgq->exitCallback = cb;
}

void emRelease(struct em*msgq) {
	aeDeleteFileEvent(msgq->base, msgq->popFd, AE_READABLE);
#ifdef LOCK
	pthread_mutex_destroy(&msgq->lock);
	circQueueRelease(msgq->queue);
#endif

#ifdef LOCKFREE
	arrayLockFreeQueueRelease(msgq->queue);
#endif

	close(msgq->pushFd);
	close(msgq->popFd);
	zfree(msgq);
}

int emPush(struct em *msgq, void *msg) {

   const char buf[1] = { 0 };
   int r = 0;

#ifdef LOCK
   pthread_mutex_lock(&msgq->lock);
   if ((r = circQueuePushTail(msgq->queue, msg)) == 0) {
      if (circQueueLength(msgq->queue) == 1) {
         send(msgq->pushFd, buf, 1,0);
      }
   }
   pthread_mutex_unlock(&msgq->lock);
#endif

#ifdef LOCKFREE
   if (arrayLockFreeQueuePush(msgq->queue, msg)) {
	   if (arrayLockFreeQueueSize(msgq->queue) == 1) {
		   send(msgq->pushFd, buf, 1, 0);
	   }
   } else {
	   fprintf(stderr, "em queue full=====\n");
	   r = -1;
   }
#endif

   return r;
}

uint32_t emLength(struct em *msgq) {
   uint32_t len = 0;
#ifdef LOCK
   //pthread_mutex_lock(&msgq->lock);
   len = circQueueLength(msgq->queue);
   //pthread_mutex_unlock(&msgq->lock);
#endif

#ifdef LOCKFREE
   len = arrayLockFreeQueueSize(msgq->queue);
#endif

   return len;
}


