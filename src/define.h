#ifndef DEFINE_H_
#define DEFINE_H_

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#if !defined(Z_PROMPT)
#define Z_PROMPT		"> "
#define Z_PROMPT2		">> "
#endif

#if !defined(Z_PROGNAME)
#define Z_PROGNAME		"z"
#endif

#if !defined(Z_MAXINPUT)
#define Z_MAXINPUT		100
#endif

#if !defined(Z_INIT)
#define Z_INIT		"Z_INIT"
#endif

#define Z_VERSION_MAJOR	"1"
#define Z_VERSION_MINOR	"0"
#define Z_VERSION_NUM		100 
#define Z_VERSION_RELEASE	"1"


#define Z_INITVERSION  \
	Z_INIT "_" Z_VERSION_MAJOR "_" Z_VERSION_MINOR

#if !defined(Z_CONTEXT_QUEUE_MAX_LENGTH)
#define CONTEXT_QUEUE_MAX_LENGTH	1024
#endif

#define Z_VERSION	0.3

#if !defined(Z_DEFINE_ENV)
#define Z_DEFINE_ENV "@.env"
#endif

#if !defined(Z_PID_PATH)
#define Z_PID_PATH "proc/"
#endif

#if !defined(Z_PID_FILENAME)
#define Z_PID_FILENAME Z_PROGNAME ".pid"
#endif

#if !defined(TCPCLIENT_QUEUE_MAX_LENGTH)
#define TCPCLIENT_QUEUE_MAX_LENGTH 1024
#endif

#if !defined(BOOTSTRAP_LOAD)
#define BOOTSTRAP_LOAD "LOAD"
#endif

#if !defined(BOOTSTRAP_NAME)
#define BOOTSTRAP_NAME "bootstrap"
#endif

#define TCP_NAME_LEN 32
#define PROTO_NAME_LEN 32
#define IPADDR_LEN		128

#define INVALID_FD -1

#define TCP_SERVER_QUEUE_LEN 1024

// system log thread queue len
#define SYS_LOG_QUEUE_LEN 1024

#define CONTEXT_QUEUE_LEN 1024

#define TCP_QUEUE_MAX_LEN	1024

#define GEN_TCP_QUEUE_MAX_LEN	1024

#if !defined(NAME_MAX_LEN)
#define NAME_MAX_LEN 31
#endif

#define CONTEXT_NAME_MAX_LEN 32

// struct message
// message.type
// 高16位预留为系统内部消息
// 低16位预留位协议组列表 也就是说只支持 25536(2^16) 中协议组
// 协议组
#define MSG_TYPE_RESPONSE		0x00000001
#define MSG_TYPE_PROTO_LUA		0x00000002
#define MSG_TYPE_PROTO_SOCKET	0x00000004

// 系统内部消息相关
#define MSG_TYPE_EXIT_CONTEXT	0xFFFFFFFF

#define MSG_TYPE_NOTCOPYDATA	0x00010000
#define MSG_TYPE_COPYDATA		0x00020000
#define MSG_TYPE_NEWSESSION		0x00080000

#define MSG_TYPE_TCP_SERVER		0x00100000
#define MSG_TYPE_TCP_CLIENT		0x00200000
#define MSG_TYPE_TCP_SEND		0x00400000
#define MSG_TYPE_TCP_CLOSE		0x00800000
#define MSG_TYPE_TCP_SERVER_OPT	0x01000000
#define MSG_TYPE_TCP_CLIENT_OPT	0x02000000

#define MSG_TYPE_SOCK_OPEN	0x00000001
#define MSG_TYPE_SOCK_DATA	0x00000002
#define	MSG_TYPE_SOCK_CLOSE	0x00000003
#define MSG_TYPE_SOCK_ACCEPT 0x00000004
#define MSG_TYPE_SOCK_CONNECT 0x00000005
#define MSG_TYPE_SOCK_ERROR 0x00000006


#define	HEAD_PROTO_TYPE 1
#define HTTP_PROTO_TYPE 2

// 日志等级类型
#define MSG_TYPE_LOG_ERROR		0
#define MSG_TYPE_LOG_WARN		1
#define MSG_TYPE_LOG_LOG		2
#define MSG_TYPE_LOG_MSG		3
#define MSG_TYPE_LOG_DEBUG		4
#define MSG_TYPE_LOG_STAT		5
#define MSG_TYPE_LOG_LEN		6

#endif

