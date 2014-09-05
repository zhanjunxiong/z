#ifndef WORKER_POOL_H_
#define	WORKER_POOL_H_

#include <stdint.h>
#include <string.h>

int workerPoolSend(struct context *ctx, 
					uint32_t from,
					uint32_t to,
					uint32_t session,
					uint32_t type,
					void *data,
					size_t sz,
					uint32_t ud);

int workerPoolStart();
void workerPoolJoin();

int workerPoolInit(int workerNum);
void workerPoolUninit();

void workerPoolStop();

#endif



