#include "socket.h"

#include "ae.h"
#include "anet.h"
#include "buffer.h"
#include "eventloop.h"
#include "define.h"
#include "proto.h"
#include "tcp.h"
#include "z.h"
#include "zmalloc.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <unistd.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>

#define READ_BUFFER_INITIAL	64
#define SEND_BUFFER_INITIAL	64


static void onAccept(struct aeEventLoop *base, 
					int fd, 
					void *ud, 
					int mask) {

	struct socket *socket = (struct socket *)ud;

	char cip[IPADDR_LEN];
	int cport;
	char neterr[ANET_ERR_LEN];
	int cfd = anetTcpAccept(neterr, fd, cip, IPADDR_LEN, &cport);
	if (cfd == AE_ERR) {
		char err[1024];
		snprintf(err, 512, "accept fail, %s", neterr);
		tcpError(socket->from, socket->id, err);
		return;
	}

	int id = tcpGetReserveId();
	if (id < 0) {
		char err[512];
		snprintf(err, 512, "socket id full");
		tcpError(socket->from, socket->id, err);
		return;
	}

	struct socket *newSocket = tcpGetSocket(id); 
	assert(newSocket != NULL);

	newSocket->from = 0;
	newSocket->fd = cfd;
	newSocket->protoType = socket->protoType;
	newSocket->type = kSocketType;
	newSocket->state = kSocketConnecting;
	if (newSocket->readBuffer) {
		bufferRetrieveAll(newSocket->readBuffer); 
	}
	if (newSocket->sendBuffer) {
		bufferRetrieveAll(newSocket->sendBuffer); 
	}

	size_t ipLen = strlen(cip);
	char *ip = (char*)zmalloc(ipLen + 1);
	zSendx(NULL, 
			0,
			socket->from,
			socket->id,
			((MSG_TYPE_SOCK_ACCEPT & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
			ip, 
			ipLen,
			newSocket->id);
}

static void onConnecting(struct aeEventLoop *base, 
						int fd, 
						void *clientData, 
						int mask) {

	struct socket *socket = (struct socket *)clientData;
	int error = 0;
	socklen_t len = sizeof(int);

	aeDeleteFileEvent(socket->eventloop, fd, AE_WRITABLE);
	if ( (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0)) {
		if (error == 0){
			// TODO: connect succ
			socket->state = kSocketConnecting;

			zSendx(NULL, 
				0,
				socket->from,
				0,
				((MSG_TYPE_SOCK_CONNECT & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
				NULL, 
				0,
				socket->id);
			return;
		}
	} 

	char err[512];
	snprintf(err, 512, "socket connect fail");
	tcpError(socket->from, socket->id, err);

	socketStop(socket);
}


static void handleClose(struct socket *socket) {
	assert(socket != NULL);

	aeDeleteFileEvent(socket->eventloop, 
					socket->fd, 
					AE_READABLE);
	aeDeleteFileEvent(socket->eventloop, 
					socket->fd, 
					AE_WRITABLE);
	close(socket->fd);

	socket->state = kSocketDisconnected;
	tcpRecoverId(socket->id);

	zSendx(NULL, 
			0,
			socket->from,
			0,
			((MSG_TYPE_SOCK_CLOSE & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
			NULL, 
			0,
			socket->id);
}

static void handleError(struct socket *socket) {
	handleClose(socket);

	char err[512];
	snprintf(err, 512, "error(%d) error str: %s\n", 
			errno,
			strerror(errno));

	tcpError(socket->from, socket->id, err);
}


static void onRead(struct aeEventLoop *base, 
							int fd, 
							void *clientData, 
							int mask) {
	struct socket *socket = (struct socket *)clientData;
	assert(socket != NULL);

	int savedErrno = 0;
	int n = socketRead(socket, &savedErrno);
	if (n > 0) {
		onMessageCallback(socket, socket->readBuffer);
	} else if (n == 0) {
		handleClose(socket);
	} else {
		errno = savedErrno;
		handleError(socket);
	}
}

static void onWrite(struct aeEventLoop *base, 
					int fd, 
					void *clientData, 
					int mask) {

	struct socket *socket = (struct socket*)clientData;
	assert(socket != NULL);

	int sendComplete = 0;
	while(1){
		int sendLen = 0;
		sendLen = send(socket->fd, 
				(void*)bufferReadIndex(socket->sendBuffer),
				bufferReadableBytes(socket->sendBuffer), 
				0);

		if (sendLen > 0){
			bufferMoveReadIndex(socket->sendBuffer, sendLen);
			if (bufferReadableBytes(socket->sendBuffer) <= 0) {
				sendComplete = 1;
				break;
			}
		} else if (sendLen == -1) {
			if (errno == EAGAIN) {
				break;
			} else {
				handleError(socket);
				return;
			}
		} else {
			break;
		}
	}

	// send complete
	if (sendComplete == 1){
		bufferRetrieveAll(socket->sendBuffer);
		// delete write event
		aeDeleteFileEvent(socket->eventloop, 
						socket->fd, 
						AE_WRITABLE);

		// TODO: write complete
	}
}



void socketInit(struct socket *socket) {
	struct aeEventLoop *eventLoop = eventloopBase();
	assert(eventLoop != NULL);

	socket->eventloop = eventLoop; 
	socket->from = 0;
	socket->protoType = 0;
	socket->id = 0;
	socket->fd = 0;
	socket->type = kSocketNoneType;
	socket->state = kSocketDisconnected;
	socket->readBuffer = NULL;
	socket->sendBuffer = NULL;
}

void socketUninit(struct socket *socket) {
	if (socket->readBuffer) {
		bufferRelease(socket->readBuffer);
	}
	if (socket->sendBuffer) {
		bufferRelease(socket->sendBuffer);
	}
}


int socketAccept(struct socket *socket) {
	assert(socket->fd > 0);

	int fd = socket->fd;
   	anetNonBlock(NULL,  fd);
	anetEnableTcpNoDelay(NULL, fd);

	return aeCreateFileEvent(socket->eventloop,
			fd,
			AE_READABLE,
			onAccept,
			socket);
}

int socketRead(struct socket *socket, int *savedErrno) {
	if (!socket->readBuffer) {
		socket->readBuffer = bufferCreate(READ_BUFFER_INITIAL);
	}

	struct buffer *buf = socket->readBuffer;

	char extrabuf[65536];
	struct iovec vec[2];
	uint32_t writeable = bufferWriteableBytes(buf);
	vec[0].iov_base = bufferWriteIndex(buf);
	vec[0].iov_len = writeable;
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof(extrabuf);

	int iovcnt = (writeable < sizeof(extrabuf))?2:1;
	ssize_t n = readv(socket->fd, vec, iovcnt);
	if (n < 0) {
		*savedErrno = errno;
	} else if (n == 0) {
		// client close
		return n;
	} else if (n <= writeable) {
		bufferMoveWriteIndex(buf, n);
	} else {
		bufferSetWriteIndex(buf, bufferGetSize(buf));			
		bufferAppend(buf, extrabuf, n - writeable);
	}

	return n;
}

int socketForceClose(struct socket *socket) {
	if (socket->state == kSocketConnected ||
		socket->state == kSocketConnecting){
		handleClose(socket);
	} else {
		tcpRecoverId(socket->id);
	}
	return 0;
}


int socketConnect(struct socket *socket) {
	return	aeCreateFileEvent(socket->eventloop, 
							socket->fd, 
							AE_WRITABLE, 
							onConnecting, 
							socket);
}

static bool hasSendBuffer(struct socket *socket) {
	if (socket->sendBuffer == NULL) {
		return false;
	}
	if (bufferReadableBytes(socket->sendBuffer) == 0) {
		return false;
	}
	return true;
}


int socketWrite(struct socket *socket, 
				const void *buf, 
				size_t len) {

	if (socket->state != kSocketConnected) {
		return -1;
	}

	bool faultError = false;
	ssize_t nwrote = 0;
	size_t remaining = len;
	if ( !hasSendBuffer(socket) ) {
		nwrote = write(socket->fd, buf, len);
		if (nwrote >= 0) {
			remaining = len - nwrote;
			if (remaining==0) {
				// TODO: write complete
			}
		} else {
			/*
			fprintf(stderr, "write error, errno: %d, strerrno(%s)\n",
				errno,
				strerror(errno));
			*/
			nwrote = 0;
			if (errno != EWOULDBLOCK) {
				if (errno == EPIPE || errno == ECONNRESET) {
					faultError = true;
				}
			}
		}
	}

	assert(remaining <= len);
	if (!faultError && remaining > 0) {
		if (!socket->sendBuffer) {
			socket->sendBuffer = bufferCreate(SEND_BUFFER_INITIAL);
		}

		bufferAppend(socket->sendBuffer, (char*)buf + nwrote, remaining);

		aeCreateFileEvent(socket->eventloop, 
						socket->fd, 
						AE_WRITABLE, 
						onWrite, 
						socket);

	}
	
	if (faultError) {
		handleError(socket);
		return -1;
	}
	return 0;
}

int socketStart(struct socket *socket) {
	int fd = socket->fd;
	assert(fd != INVALID_FD);

	anetNonBlock(NULL, fd);
	anetEnableTcpNoDelay(NULL, fd);

	socket->state = kSocketConnected;
	return aeCreateFileEvent(socket->eventloop, 
						fd, 
						AE_READABLE, 
						onRead, 
						socket);

}

void socketStop(struct socket *socket) {
	if (socket->fd > 0) {
		close(socket->fd);
	}
	socket->state = kSocketDisconnected;
	tcpRecoverId(socket->id);
}


