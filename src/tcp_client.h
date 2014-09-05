#ifndef TCPCLIENT_H_
#define TCPCLIENT_H_

#include "callback.h"

struct tcpClient {
	int retryDelayMs;
	int retry;
};

extern struct netCallback tcpClientCallback;

#endif 

