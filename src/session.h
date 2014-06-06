#ifndef SESSION_H_
#define SESSION_H_

#include "callback.h"

#define IPADDR_LEN		128

#define SESSION_NONE	 	0
#define SESSION_READABLE 	1        //0001B
#define SESSION_WRITEABLE 	2		 //0010B

struct buffer;
struct session;

enum StateE { 
	kDisconnected, 
	kConnecting, 
	kConnected, 
	kDisConnecting 
};

struct session {
	uint32_t id;
	int cfd;
	uint32_t mask;

	int cport;
	char cip[IPADDR_LEN];
	enum StateE	 state;

	struct buffer* readBuffer;
	struct buffer* sendBuffer;

	struct aeEventLoop* base;

	struct netCallback onNetCallback;
	void* privateData;

	union {
		void* val;
		struct tcpServer* srv;
		struct tcpClient* client;
	} owner;
	// 协议名字
	// 只是个指针,这个对象由tcpserver释放
	char* protoName;
};

struct session* sessionCreate();
void sessionRelease(struct session* session);

int32_t sessionRead(struct session* session, int*);
int32_t sessionWrite(struct session* session, const void* buf, size_t len);

void sessionUpdateEvent(struct session* session);
void sessionRemoveEvent(struct session* session); 
void sessionCloseWriteEvent(struct session* session);

void sessionForceClose(struct session* session); 

uint32_t newSessionid();

#endif 


