#include "event_msgqueue.h"

#include "ae.h"
#include "circqueue.h"
#include "zmalloc.h"

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <pthread.h>
#include <unistd.h>

struct em{
   int pushFd;
   int popFd;
   int unlockBetweenCallbacks;
   int mask;

   struct aeEventLoop *base;

   pthread_mutex_t lock;

   eventCallback callback;
   void *cbarg;
   struct circQueue *queue;
};


static void popMsgqueue(aeEventLoop* el, int fd, void *arg, int mask) {

   struct em *msgq = (struct em*)arg;
   char buf[64];

   recv(fd, buf, sizeof(buf),0);

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
}

struct em* emCreate(struct aeEventLoop* base, 
			unsigned int maxSize, 
			eventCallback callback,
			void* cbarg) {
	assert(base != NULL);
	
	struct em* msgq;
	struct circQueue* cq;
	int fds[2];

	if (!(cq = circQueueCreate(maxSize)))
		return NULL;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
		circQueueRelease(cq);
		return NULL;
	}

	if (!(msgq = (struct em*)zmalloc(sizeof(*msgq)))) {
		circQueueRelease(cq);
      	close(fds[0]);
      	close(fds[1]);
      	return NULL;
   	}

	msgq->pushFd = fds[0];
	msgq->popFd = fds[1];
	msgq->queue = cq;
	msgq->callback = callback;
	msgq->cbarg = cbarg;
	pthread_mutex_init(&msgq->lock, NULL);

	msgq->base = base;
	msgq->unlockBetweenCallbacks = 1;

	aeCreateFileEvent(base, msgq->popFd, AE_READABLE, popMsgqueue, msgq);
	return(msgq);
}

void emRelease(struct em*msgq) {
	aeDeleteFileEvent(msgq->base, msgq->popFd, AE_READABLE);
	pthread_mutex_destroy(&msgq->lock);
	circQueueRelease(msgq->queue);
	close(msgq->pushFd);
	close(msgq->popFd);
	zfree(msgq);
}

int emPush(struct em *msgq, void *msg) {
   const char buf[1] = { 0 };
   int r = 0;

   pthread_mutex_lock(&msgq->lock);
   if ((r = circQueuePushTail(msgq->queue, msg)) == 0) {
      if (circQueueLength(msgq->queue) == 1) {
         send(msgq->pushFd, buf, 1,0);
      }
   }
   pthread_mutex_unlock(&msgq->lock);

   return r;
}

uint32_t emLength(struct em *msgq) {
   uint32_t len = 0;
   pthread_mutex_lock(&msgq->lock);
   len = circQueueLength(msgq->queue);
   pthread_mutex_unlock(&msgq->lock);
   return len;
}

