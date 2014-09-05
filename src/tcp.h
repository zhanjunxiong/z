#ifndef TCP_H_
#define TCP_H_

#include <string.h> // for size_t

int tcpRecoverId(int id);
int tcpGetReserveId();

void tcpError(int to, int id, char *err);
struct socket *tcpGetSocket(int id);

int tcpSocketListen(struct context *ctx,
				int protoType,
				const char *addr,
				int port);

int tcpSocketStart(struct context *ctx,
					int id);

int tcpSocketConnect(struct context *ctx,
					int protoType,
					const char *addr,
					int port);
int tcpSocketAsyncConnect(struct context *ctx,
					int protoType,
					const char *addr,
					int port);

int tcpSocketClose(struct context *ctx,
					int id);

int tcpSocketSend(struct context *ctx, 
				int ids[], 
				int idNum, 
				void *data, 
				size_t sz);

void tcpStop();

int tcpInit(int setsize);
void tcpUninit();

#endif

