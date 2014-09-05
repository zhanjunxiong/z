#ifndef GEN_TCP_H_
#define GEN_TCP_H_

#include "define.h"

struct listenRequest{
	char addr[NAME_MAX_LEN + 1];
	int port;
	int protoType;
	int backlog;
};

struct startRequest {
	int id;
};

struct connectRequest{
	char addr[NAME_MAX_LEN + 1];
	int port;
	int protoType;
	int retry;
};

struct sendRequest {
	size_t sz;
	void *data;
	int idNum;
};

struct closeRequest {
	int id;
};

enum socketRequestE {
	kListen,
	kStart,
	kConnect,
	kSend,
	kClose,
	kExit,
};

struct socketRequest {
	int from;
	int sessionid;
	int id;
	enum socketRequestE type;
	union {
		struct listenRequest listenRequest;
		struct startRequest startRequest;
		struct connectRequest connectRequest;
		struct sendRequest sendRequest;
		struct closeRequest closeRequest;
	} u;
	int ids[];
};

struct socketMessage {
	size_t sz;
	void *data;
	int ud;
	int id;
	int type;
};

int getReserveId();
struct session *newSession(struct aeEventLoop *eventloop, int id, int fd); 
void removeSession(int id); 

uint32_t buildSessionUd(struct session *session);

int genTcpListen(struct context *ctx,
				int protoType,
				const char *addr,
				int port);

int genTcpStartSocket(struct context *ctx,
						int id);

int genTcpConnect(struct context *ctx,
				int protoType,
				const char *addr,
				int port,
				int retry);

int genTcpSend(struct context *ctx, 
				int ids[], 
				int idNum, 
				void *data, 
				size_t sz);

int genTcpCloseSocket(struct context *ctx,
					int id);

int genTcpInit(int setsize);
void genTcpUninit();

void genTcpStop();


#endif

