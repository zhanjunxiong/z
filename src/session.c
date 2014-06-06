#include "session.h"

#include "ae.h"
#include "buffer.h"
#include "tcp_server.h"
#include "zmalloc.h"

#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#define READ_BUFFER_INITIAL									1024
#define SEND_BUFFER_INITIAL									1024

static uint32_t SID = 1;
uint32_t newSessionid() {
	// session always be a positive number
	int sessionid = (++SID) & 0x7fffffff;
	return sessionid;
}

struct session* sessionCreate() {
	struct session *session = (struct session*)zcalloc(sizeof(*session));
	session->cfd = -1;
	session->mask = SESSION_READABLE; //默认可读？？
	session->state = kConnecting; 
	session->id = newSessionid();
	return session;
}

void sessionRelease(struct session* session) {
	session->state = kDisconnected;
	session->mask = SESSION_NONE;
	if (session->readBuffer) {
		bufferRelease(session->readBuffer);
	}
	if (session->sendBuffer) {
		bufferRelease(session->sendBuffer);
	}

	if (session->cfd >= 0) {
		close(session->cfd);
	}

	zfree(session);
}

int32_t sessionRead(struct session* session, int* savedErrno) {
	if (!session->readBuffer) {
		session->readBuffer = bufferCreate(READ_BUFFER_INITIAL);
	}

	struct buffer* buf = session->readBuffer;

	char extrabuf[65536];
	struct iovec vec[2];
	uint32_t writeable = bufferWriteableBytes(buf);
	vec[0].iov_base = bufferWriteIndex(buf);
	vec[0].iov_len = writeable;
	vec[1].iov_base = extrabuf;
	vec[1].iov_len = sizeof(extrabuf);
	int iovcnt = (writeable < sizeof(extrabuf))?2:1;
	ssize_t n = readv(session->cfd, vec, iovcnt);
	if (n < 0) {
		*savedErrno = errno;
	} else if (n <= writeable) {
		bufferMoveWriteIndex(buf, n);
	} else {
		bufferSetWriteIndex(buf, bufferGetSize(buf));			
		bufferAppend(buf, extrabuf, n - writeable);
	}

	return n;
}

int32_t sessionWrite(struct session* session, const void* buf, size_t len) {
	if (session->state != kConnected) {
		return -1;
	}

	bool faultError = false;
	ssize_t nwrote = 0;
	size_t remaining = len;
	if (!(session->mask & SESSION_WRITEABLE)) {
		nwrote = write(session->cfd, buf, len);
		if (nwrote >= 0) {
			remaining = len - nwrote;
			if (remaining && session->onNetCallback.onWriteComplete) {
				session->onNetCallback.onWriteComplete(session);
			}
		} else {
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
		session->mask = session->mask & SESSION_WRITEABLE;
		sessionUpdateEvent(session);
	}
	
	return 0;
}

static void _onWriteSession(struct aeEventLoop* base, int fd, void* clientData, int mask) {
	struct session* session = (struct session*)clientData;
	if (!session)
		return;

	int sendComplete = 0;
	while(1){
		int sendLen = 0;
		sendLen = send(session->cfd, 
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
				break;
			}
		} else {
			break;
		}
	}

	// 已经写完成了
	if (sendComplete == 1){
		if (session->onNetCallback.onWriteComplete) {
			session->onNetCallback.onWriteComplete(session);
		}
		// 写完 关掉写事件
		sessionCloseWriteEvent(session);
	}
}

void sessionHandleError(struct session* session) {
}

void sessionHandleClose(struct session* session) {
	assert(session->state == kConnected || session->state == kConnecting);

	sessionRemoveEvent(session);
	if (session->onNetCallback.onClose){
		session->onNetCallback.onClose(session);
	}
}

void sessionForceClose(struct session* session) {
	if (session->state == kConnected ||
			session->state == kDisConnecting ){
		sessionHandleClose(session);
	}
}


static void _onReadSession(struct aeEventLoop* base, int fd, void* clientData, int mask) {
	struct session* session = (struct session*)clientData;
	assert(session != NULL);

	int savedErrno = 0;
	int32_t n = sessionRead(session, &savedErrno);
	if (n > 0) {
		if (session->onNetCallback.onMessage){
			session->onNetCallback.onMessage(session, session->readBuffer);
		}
	} else if (n == 0) {
		sessionHandleClose(session);
	} else {
		errno = savedErrno;
		fprintf(stderr, "read session error, error str: %s\n", strerror(errno));
		sessionHandleError(session);
	}
}

void sessionUpdateEvent(struct session* session) {
	if (session->mask & SESSION_WRITEABLE){
		aeCreateFileEvent(session->base, session->cfd, AE_WRITABLE, _onWriteSession, session);
	}
	if (session->mask & SESSION_READABLE) {
		aeCreateFileEvent(session->base, session->cfd, AE_READABLE, _onReadSession, session);
	}
}

void sessionRemoveEvent(struct session* session) {
	if (session->state != kConnected)
		return;

	if (session->mask & SESSION_WRITEABLE){
		aeDeleteFileEvent(session->base, session->cfd, AE_WRITABLE);
	}
	if (session->mask & SESSION_READABLE) {
		aeDeleteFileEvent(session->base, session->cfd, AE_READABLE);
	}
}

void sessionCloseWriteEvent(struct session* session){
	aeDeleteFileEvent(session->base, session->cfd, AE_WRITABLE);
	bufferRetrieveAll(session->sendBuffer);
	if (session->mask == SESSION_NONE) return;
	session->mask = session->mask &(~SESSION_WRITEABLE);
}

