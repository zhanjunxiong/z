#ifndef SOCKET_H_
#define	SOCKET_H_

#include <string.h> // for size_t

enum socketTypeE {
	kSocketNoneType,
	kReserveType,
	kAcceptType,
	kSocketType,
	kConnectType,
};

enum socketStateE { 
	kSocketDisconnected, 
	kSocketConnecting, 
	kSocketConnected, 
	kSocketDisConnecting 
};

struct socket {
	struct aeEventLoop *eventloop;

	int from;
	int protoType;
	int id;
	int fd;

	enum socketTypeE type;
	enum socketStateE state;

	struct buffer *readBuffer;
	struct buffer *sendBuffer;
};

void socketInit(struct socket *socket);
void socketUninit(struct socket *socket);

int socketStart(struct socket *socket);
void socketStop(struct socket *socket);

int socketAccept(struct socket *socket); 
int socketConnect(struct socket *socket); 
int socketForceClose(struct socket *socket);

int socketRead(struct socket *socket, int *savedErrno); 
int socketWrite(struct socket *socket, 
				const void *buf, 
				size_t len); 


#endif

