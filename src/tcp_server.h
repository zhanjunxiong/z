#ifndef TCPSERVER_H_
#define TCPSERVER_H_

#include "callback.h"
#include "tcp_message.h"

struct dict;
struct tcpServer {
	struct aeEventLoop* base;

	int listenfd;
	int listenPort;
	char* listenAddr;
	char* name;

	struct dict* sessions;
	struct em* em;
	void* privateData;

	struct netCallback onNetCallback;
	// 协议名字
	char* protoName;
	// tcpserver 状态
	// 0 还没初始化
	// 1 开启
	// 2 关闭
	int state;
};


struct tcpServer* tcpServerCreate(const char* srvName,
									const char* listenAddr, 
									int listenPort);
void tcpServerRelease(struct tcpServer* srv);

int tcpServerStart(struct tcpServer* srv);
void tcpServerStop(struct tcpServer* srv);

void tcpServerConnectionCallback(struct tcpServer* srv, connectionCallback cb);
void tcpServerCloseCallback(struct tcpServer* srv, closeCallback cb);
void tcpServerWriteCompleteCallback(struct tcpServer* srv, writeCompleteCallback cb);
void tcpServerHightWaterMarkCallback(struct tcpServer* srv, hightWaterMarkCallback cb);

void tcpServerMessageCallback(struct tcpServer* srv, messageCallback cb);
void tcpServerProtoMessageCallback(struct tcpServer* srv, protoMessageCallback cb);

void tcpServerPrivateData(struct tcpServer* srv, void* privateData);

void tcpServerSetProto(struct tcpServer* srv, const char* proto);


struct tcpServer* getTcpServer(const char* name);
int tcpServerSend(const char* srvName, uint32_t id, int type, void* data, size_t sz);

int tcpServerInit();
void tcpServerUninit();


#endif 


