#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include "callback.h"
#include "tcp_message.h"

#include <stdint.h>
#include <stdlib.h>

struct aeEventLoop;
struct session;

enum tcpClientStates { 
	tcpClient_kDisconnected, 
	tcpClient_kConnecting, 
	tcpClient_kConnected 
};

struct tcpClient {
	struct aeEventLoop* base;
	char* serverAddr;
	uint32_t serverPort;
	uint32_t retryDelayMs;
	uint32_t retry;
	struct session* session;
	enum tcpClientStates state;

	struct netCallback onNetCallback;
	struct em* em;
	void* privateData;

	char* name;
	// 协议名字
	char* protoName;
};

struct tcpClient* tcpClientCreate(const char* name, const char* serverAddr, uint32_t serverPort);
void tcpClientRelease(struct tcpClient* tcpClient);

void tcpClientConnectionCallback(struct tcpClient* client, connectionCallback cb);
void tcpClientCloseCallback(struct tcpClient* client, closeCallback cb);
void tcpClientWriteCompleteCallback(struct tcpClient* client, writeCompleteCallback cb);
void tcpClientHightWaterMarkCallback(struct tcpClient* client, hightWaterMarkCallback cb);

void tcpClientMessageCallback(struct tcpClient* client, messageCallback cb);
void tcpClientProtoMessageCallback(struct tcpClient* client, protoMessageCallback cb);

void tcpClientPrivateData(struct tcpClient* client, void* privateData);
void tcpClientSetProto(struct tcpClient* client, const char* proto);

int tcpClientStart(struct tcpClient* tcpClient);
void tcpClientStop(struct tcpClient* tcpClient);

struct tcpClient* getTcpClient(const char* name); 
int tcpClientSend(const char* clientName, int type, void* data, size_t sz);

int tcpClientInit();
void tcpClientUninit();

#endif 

