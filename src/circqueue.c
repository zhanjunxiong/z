#include "circqueue.h"

#include "zmalloc.h"

#include <stdio.h>
#include <string.h>

typedef struct circQueue {
   uint32_t head;
   uint32_t tail;
   uint32_t count;
   uint32_t maxEntries;
   uint32_t arrayElements;
   void **entries;
} circQueue;

#define DEFAULT_UNBOUNDED_QUEUE_SIZE 1024
static uint32_t nextpow2(uint32_t num) {
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return ++num;
}

struct circQueue* circQueueCreate(uint32_t size) {
   struct circQueue *cq = NULL;

   if (!(cq = (struct circQueue*)zcalloc(sizeof(*cq)))) {
      return NULL;
   }

   cq->maxEntries = size;
   if (!size || !(cq->arrayElements = nextpow2(size))) {
      cq->arrayElements = DEFAULT_UNBOUNDED_QUEUE_SIZE;
   }

   cq->entries = (void**)zcalloc(sizeof(void *) * cq->arrayElements);
   if (!cq->entries) {
      zfree(cq);
      return NULL;
   }

   return cq;
}

void circQueueRelease(struct circQueue *cq) {
   zfree(cq->entries);
   zfree(cq);
}

uint32_t circQueueLength(struct circQueue* cq) {
	return cq->count;
}

bool circQueueIsEmpty(struct circQueue* cq) {
	return cq->count == 0;
}

bool circQueueIsFull(struct circQueue* cq) {
	return cq->count == cq->arrayElements;
}

int _circQueueGrow(struct circQueue *cq) {
   void **newents;
   uint32_t newsize = cq->arrayElements << 1;
   uint32_t headchunklen = 0, tailchunklen = 0;
   
   if (!(newents = (void**)zcalloc(sizeof(void *) * newsize))) {
      return(-1);
   }

   if (cq->head < cq->tail) {
      headchunklen = cq->tail - cq->head;
   }
   else {
      headchunklen = cq->arrayElements - cq->head;
      tailchunklen = cq->tail;
   }

   memcpy(newents, &cq->entries[cq->head], sizeof(void *) * headchunklen);
   if (tailchunklen) {
      memcpy(&newents[headchunklen], cq->entries, sizeof(void *) * tailchunklen);
   }

   cq->head = 0;
   cq->tail = headchunklen + tailchunklen;
   cq->arrayElements = newsize;

   zfree(cq->entries);
   cq->entries = newents;

   return 0;
}

int circQueueForcePushTail(struct circQueue* cq, void* elem) {
	int ret = circQueuePushTail(cq, elem);
	if (ret == 0) {
		return ret;
	}

	if (_circQueueGrow(cq) == 0) {
		ret = circQueuePushTail(cq, elem);
	} else {
		return -1;
	}

	return ret;
}

int circQueuePushTail(struct circQueue *cq, void *elem) {
   if (cq->maxEntries) {
      if (cq->count == cq->maxEntries) {
		  fprintf(stderr, "count: %d, maxEntries: %d\n", cq->count, cq->maxEntries);
         return -2;
      }
   } else if (circQueueIsFull(cq) && _circQueueGrow(cq) != 0) {
      return -1;
   }

   cq->count++;
   cq->entries[cq->tail++] = elem;
   cq->tail &= cq->arrayElements - 1;

   return 0;
}

void* circQueuePopHead(struct circQueue *cq) {
   void *data = NULL;
   if (cq->count == 0) {
      return NULL;
   }
   
   cq->count--;
   data = cq->entries[cq->head++];
   cq->head &= cq->arrayElements - 1;

   return data;
}

void circQueueClear(struct circQueue *cq) {
	cq->count = 0;
	cq->head = 0;
	cq->tail = 0;
}

