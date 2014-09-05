#include "session.h"

#include "ae.h"
#include "anet.h"
#include "buffer.h"
#include "log.h"
#include "gen_tcp.h"
#include "protocol.h"
#include "zmalloc.h"
#include "z.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define READ_BUFFER_INITIAL	64
#define SEND_BUFFER_INITIAL	64

static void handleClose(struct session *session) {
	assert(session != NULL);

	sessionSetState(session, kDisConnecting);
}

static void handleError(struct session *session) {
	fprintf(stderr, "session error(%d) error str: %s\n", 
			errno,
			strerror(errno));
	handleClose(session);
}

static void onAccept(struct aeEventLoop *base, 
					int fd, 
					void *ud, 
					int mask) {

	struct session *session = (struct session*)ud;

	char cip[IPADDR_LEN];
	int cport;
	char neterr[256];
	int cfd = anetTcpAccept(neterr, fd, cip, IPADDR_LEN, &cport);
	if (cfd == AE_ERR) {
		// TODO: add commit
		logInfo("onAccept, accept fail");	
		return;
	}

	int id = getReserveId();
	if (id < 0) {
		fprintf(stderr, "get reserve id error, id: %d\n", id);
		return;
	}

	struct session *newsession = newSession(session->eventloop, id, INVALID_FD);
	if (newsession == NULL) {
		return;
	}

	newsession->type = kSessionSocketType;
	newsession->cb = session->cb;
	newsession->protoType = session->protoType;
	newsession->port = cport;
	newsession->destId = session->id;
	memcpy(newsession->ip, cip, IPADDR_LEN);
	newsession->fd = cfd;
	
	//newsession->from = session->from;
	//sessionBindfd(newsession, cfd);
	zSendx(NULL, 
			0,
			session->from,
			session->id,
			((MSG_TYPE_SOCK_ACCEPT & 0xFFFF) << 16) | MSG_TYPE_PROTO_SOCKET,
			NULL, 
			0,
			newsession->id);
}


static void onConnecting(struct aeEventLoop *base, 
						int fd, 
						void *clientData, 
						int mask) {

	struct session *session = (struct session*)clientData;
	int error = 0;
	socklen_t len = sizeof(int);

	aeDeleteFileEvent(session->eventloop, fd, AE_WRITABLE);
	if ( (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) == 0)) {
		if (error == 0){
			// TODO: 
			sessionBindfd(session, fd); 
			return;
		}
	} 

	session->fd = fd;
	sessionSetState(session, kDisConnecting);
}

static void onRead(struct aeEventLoop *base, 
							int fd, 
							void *clientData, 
							int mask) {
	struct session *session = (struct session*)clientData;
	assert(session != NULL);

	int savedErrno = 0;
	int n = sessionRead(session, &savedErrno);
	if (n > 0) {
		defaultMessageCallback(session, session->readBuffer);
	} else if (n == 0) {
		handleClose(session);
	} else {
		errno = savedErrno;
		handleError(session);
	}
}

static void onWrite(struct aeEventLoop *base, 
					int fd, 
					void *clientData, 
					int mask) {

	struct session *session = (struct session*)clientData;
	assert(session != NULL);

	int sendComplete = 0;
	while(1){
		int sendLen = 0;
		sendLen = send(session->fd, 
				(void*)bufferReadIndex(session->sendBuffer),
				bufferReadableBytes(session->sendBuffer), 
				0);

		if (sendLen > 0){
			bufferMoveReadIndex(session->sendBuffer, sendLen);
			if (bufferReadableBytes(session->sendBuffer) <= 0) {
				sendComplete = 1;
				break;
			}
		} else if (sendLen == -1) {
			if (errno == EAGAIN) {
				break;
			} else {
				handleError(session);
				return;
			}
		} else {
			break;
		}
	}

	// send complete
	if (sendComplete == 1){
		bufferRetrieveAll(session->sendBuffer);
		// delete write event
		aeDeleteFileEvent(session->eventloop, 
						session->fd, 
						AE_WRITABLE);

		defaultWriteCompleteCallback(session);
	}
}

struct session* sessionCreate(struct aeEventLoop *eventloop, int id, int fd) {
	struct session *session = (struct session*)zmalloc(sizeof(*session));

	session->eventloop = eventloop;
	session->id = id;
	session->fd = INVALID_FD;
	session->port = 0;
	session->ip[0] = '\0';
	session->state = kDisconnected; 
	session->type = kSessionTypeNONE;

	session->readBuffer = NULL;
	session->sendBuffer = NULL;

	sessionBindfd(session, fd);
	//printf("session create, id: %d\n", id);
	return session;
}

void sessionRelease(struct session* session) {

	//printf("session release, id: %d\n", session->id);

	if (session->state == kConnected ||
		session->state == kConnecting) {
		sessionSetState(session, kDisConnecting);
	}

	if (session->readBuffer) {
		bufferRelease(session->readBuffer);
	}
	if (session->sendBuffer) {
		bufferRelease(session->sendBuffer);
	}
	zfree(session);
}



void sessionSetState(struct session *session, enum StateE state) {
	session->state = state;
	if (state == kConnected) {
		aeCreateFileEvent(session->eventloop, 
						session->fd, 
						AE_READABLE, 
						onRead, 
						session);

		// TODO: connect
		defaultConnectionCallback(session);
	} else if (state == kConnecting) {
		aeCreateFileEvent(session->eventloop, 
						session->fd, 
						AE_WRITABLE, 
						onConnecting, 
						session);
	} else if (state == kDisConnecting) {

		aeDeleteFileEvent(session->eventloop, 
						session->fd, 
						AE_READABLE);

		aeDeleteFileEvent(session->eventloop, 
						session->fd, 
						AE_WRITABLE);

		close(session->fd);
		// TODO: on close
		defaultCloseCallback(session);
		//session->fd = INVALID_FD;
	} else if (state == kDisconnected) {
	}
}

int sessionBindfd(struct session *session, int fd) {
	assert(session->fd == INVALID_FD);

	if (fd == INVALID_FD) {
		return -1;
	}

	anetNonBlock(NULL, fd);
	anetEnableTcpNoDelay(NULL, fd);

	session->fd = fd;
	sessionSetState(session, kConnected);
	return 0;
}

int sessionStart(struct session *session) {
	assert(session->fd != INVALID_FD);

	anetNonBlock(NULL, session->fd);
	anetEnableTcpNoDelay(NULL, session->fd);

	sessionSetState(session, kConnected);
	return 0;
}


int sessionRead(struct session *session, int *savedErrno) {
	if (!session->readBuffer) {
		session->readBuffer = bufferCreate(READ_BUFFER_INITIAL);
	}

	struct buffer *buf = session->readBuffer;

	char extrabuf[65536];
	struct iovec vec[2];
	uint32_t writeable = bufferWriteableBytes(buf);
	vec[0].iov_base = bufferWriteIndex(buf);
	vec[0].iov_len = writeable;
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof(extrabuf);

	int iovcnt = (writeable < sizeof(extrabuf))?2:1;
	ssize_t n = readv(session->fd, vec, iovcnt);
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

static bool hasSendBuffer(struct session* session) {
	if (session->sendBuffer == NULL) {
		return false;
	}
	if (bufferReadableBytes(session->sendBuffer) == 0) {
		return false;
	}
	return true;
}

int sessionWrite(struct session *session, 
				const void *buf, 
				size_t len) {

	if (session->state != kConnected) {
		return -1;
	}

	bool faultError = false;
	ssize_t nwrote = 0;
	size_t remaining = len;
	if ( !hasSendBuffer(session) ) {
		nwrote = write(session->fd, buf, len);
		if (nwrote >= 0) {
			remaining = len - nwrote;
			if (remaining==0) {
				defaultWriteCompleteCallback(session);
			}
		} else {
			fprintf(stderr, "write error, errno: %d, strerrno(%s)\n",
				errno,
				strerror(errno));

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
		if (!session->sendBuffer) {
			session->sendBuffer = bufferCreate(SEND_BUFFER_INITIAL);
		}

		bufferAppend(session->sendBuffer, (char*)buf + nwrote, remaining);

		aeCreateFileEvent(session->eventloop, 
						session->fd, 
						AE_WRITABLE, 
						onWrite, 
						session);

	}
	
	if (faultError) {
		handleError(session);
		return -1;
	}
	return 0;
}

int sessionForceClose(struct session* session) {
	if (session->state == kConnected ||
		session->state == kConnecting){
		handleClose(session);
	} 
	return 0;
}

void sessionSetud(struct session *session, void *ud) {
	session->ud = ud;
}

void sessionAccept(struct session *session, int fd) {
	session->fd = fd;
   	anetNonBlock(NULL,  fd);
	anetEnableTcpNoDelay(NULL, fd);

	aeCreateFileEvent(session->eventloop,
			fd,
			AE_READABLE,
			onAccept,
			session);
}

int sessionConnect(struct session *session) {
	char err[ANET_ERR_LEN];
	int fd = anetTcpNonBlockConnect(err, 
							session->ip, 
							session->port);
	if (fd == ANET_ERR) {
		return -1;
	}

	if (errno == EINPROGRESS) {
		aeCreateFileEvent(session->eventloop, 
							fd, 
							AE_WRITABLE, 
							onConnecting, 
							session);
		return 0;
	}
	
	// connect succ
	sessionBindfd(session, fd);
	return 0;
}

void sessionResetReadBuffer(struct session *session) {
	bufferReset(session->readBuffer);
}

