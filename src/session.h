#ifndef SESSION_H_
#define SESSION_H_

#include "define.h"

struct aeEventLoop;
struct netCallback;
struct buffer;

enum StateE { 
	kDisconnected, 
	kConnecting, 
	kConnected, 
	kDisConnecting 
};

enum SessionTypeE {
	kSessionTypeNONE,
	kSessionAcceptType,
	kSessionSocketType,
	kSessionConnectType,
};

struct session {
	int from;
	enum SessionTypeE type;

	struct aeEventLoop *eventloop;
	struct netCallback *cb;

	int protoType;
	int destId;

	int id;
	int fd;
	int port;
	char ip[IPADDR_LEN];

	enum StateE state;

	struct buffer *readBuffer;
	struct buffer *sendBuffer;

	void *ud;
};

struct session *sessionCreate(struct aeEventLoop *eventloop, int id, int fd);
void sessionRelease(struct session *session);

void sessionResetReadBuffer(struct session *session);

void sessionAccept(struct session *session, int fd);
int sessionConnect(struct session *session);
int sessionStart(struct session *session);

void sessionSetState(struct session *session, enum StateE state);
void sessionSetud(struct session *session, void *ud);

int sessionRead(struct session *session, int*);
int sessionWrite(struct session *session, const void *buf, size_t len);

int sessionForceClose(struct session *session); 

int sessionBindfd(struct session *session, int fd);

#endif 

