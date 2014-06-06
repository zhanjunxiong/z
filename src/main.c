
#include "ae.h"
#include "config.h"
#include "ctx_mgr.h"
#include "daemon.h"
#include "eventloop.h"
#include "blockqueue.h"
#include "module.h"
#include "queue.h"
#include "context.h"
#include "log.h"
#include "event_msgqueue.h"
#include "globalqueue.h"
#include "thread.h"
#include "tcp_server.h"
#include "tcp_client.h"
#include "timer.h"
#include "zmalloc.h"

#include <pthread.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>


volatile bool shutdown = false;
static void stop() {
	shutdown = true;
	eventloopStop();
	timerStop();
}

static void sigterHandler(int sig) {
	stop();	
	return;
}

static void setupSignalHandlers() {
	struct sigaction act;
	
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = sigterHandler;
	sigaction(SIGTERM, &act, NULL);

#ifdef HAVE_BACKTRACE
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_NODEFER | SA_RESETHAND | SA_SIGINFO;
	act.sa_sigaction = sigsegvHandler;
	sigaction(SIGSEGV, &act, NULL);
	sigaction(SIGBUS, &act, NULL);
	sigaction(SIGFPE, &act, NULL);
	sigaction(SIGILL, &act, NULL);
#endif
}


static int messageDispatch() {
	struct context* ctx = globalqueueTake(); 
	if (ctx==NULL)
		return 1;
	if (ctx->state == 2)
		return 1;

	if (contextDispatchMessage(ctx) < 0) {
		// 该上下位暂时没有事情做
		__sync_lock_test_and_set(&ctx->state, 0);	
	} else {
		// 把ctx 重新放到调度队列中
		globalqueuePut(ctx);
	}
	contextRelease(ctx);
	return 0;
}

static void* timer() {
	timerMain();

	globalqueueExit();
	return NULL;
}

static void* worker() {
	while (true){
		if (messageDispatch()) {
			if (ctxMgrHasWork() == 0){
				// 所有服务都处理完了,在退出循环
				globalqueueExit();
				break;
			} else {
				fprintf(stderr, "length: %d\n", globalqueueLength());
			}
		}
	}
	fprintf(stderr, "work exit\n");
	return NULL;
}

static void start() {
	int threadNum = configGetInt("thread_num", 4);

	struct thread** threads = (struct thread**)zmalloc(threadNum*sizeof(struct thread*));
	int i;
	char name[50];
	for (i=0; i < threadNum; i++) {
		snprintf(name, 49, "workThread_%d", i);
		threads[i] = threadCreate(worker, name);
	}

	for (i=0; i < threadNum; i++) {
		threadStart(threads[i]);
	}
	struct thread* timerThread = threadCreate(timer, "timer");
	threadStart(timerThread);

	eventloopStart();

	threadJoin(timerThread);

	threadRelease(timerThread);
	for (i=0; i < threadNum; i++) {
		threadJoin(threads[i]);
		threadRelease(threads[i]);
	}
	zfree(threads);
}

static void showVersion() {
	fprintf(stdout, "z version is 0.1\n");
	exit(1);
}

static void showHelp() {
	fprintf(stdout, "help is nil\n");
	exit(1);
}

void vperror(const char *fmt, ...) {
	int old_errno = errno;
	char buf[1024];
	va_list ap;

	va_start(ap, fmt);
	if (vsnprintf(buf, sizeof(buf), fmt, ap) == -1) {
		buf[sizeof(buf) - 1] = '\0';
	}
	va_end(ap);
	errno = old_errno;
	perror(buf);
}

static void savePid(const pid_t pid, const char *pidFile) {
	FILE *fp;
	if (pidFile == NULL)
		return;

	if ((fp = fopen(pidFile, "w")) == NULL) {
		vperror("Could not open the pid file %s for writing", pidFile);
		return;
	}

	fprintf(fp,"%ld\n", (long)pid);
	if (fclose(fp) == -1) {
		vperror("Could not close the pid file %s", pidFile);
		return;
	}
}

static void removePidfile(const char *pidFile) {
	if (pidFile == NULL)
		return;

	if (unlink(pidFile) != 0) {
		vperror("Could not remove the pid file %s", pidFile);
	}
}

static void init() {
	const char* modulePath = configGet("modulePath", "./cservice/?.so;");

	globalqueueInit();
	ctxMgrInit();
	eventloopInit();
	tcpServerInit();
	tcpClientInit();
	timerInit();
	moduleInit(modulePath);
}

static void bootstrap() {
	const char* mainModName = configGet("bootstrap", "bootstrap");
	struct context* ctx = contextCreate("bootstrap", "zlua", mainModName);
	if (ctx == NULL) {
		fprintf(stderr, "load zlua main fail\n");
		exit(1);
	}
}

static void uninit() {
	tcpServerUninit();
	tcpClientUninit();
	eventloopUninit();
	globalqueueUninit();
	timerUninit();
	ctxMgrUninit();

	moduleUninit();
}

int main(int argc, char **argv) {
	// 
	char* workDir = getenv("Z");
	if (workDir) {
		chdir(workDir);
	}
	// 
	char path[FILENAME_MAX];
	if(!getcwd(path, FILENAME_MAX)) {
		fprintf(stderr, "z getcwd error\n");
		return -1;
	}

	const char* configFileName = "config";
	int o;
	int maxcore = 0;
	bool do_daemonize = false;
	char* pidFileName = NULL;
	while(-1 != (o = getopt(argc, argv,
					"h" // show help
					"f:" // config file name
					"d" // run as a daemon
					"v" // show version
					"P:" // pid file name
					))) {
		switch(o) {
			case 'v':
				showVersion();
				break;
			case 'h':
				showHelp();
				break;
			case 'd':
				do_daemonize = true;
				break;
			case 'f':
				configFileName = optarg;
				break;
			case 'P':
				pidFileName = optarg;
			default:
				showHelp();
				return -1;
		}
	}

	if (do_daemonize) {
		if (daemonize(maxcore, 0) == -1) {
			fprintf(stderr, "failed to daemon() in order to daemonize\n");
			return -1;
		}

		savePid(getpid(), pidFileName);
	}

	setupSignalHandlers();

	// init config
	configInit(configFileName);

	//logInit();

	init();
	bootstrap();
	start();
	uninit();

	//logUninit();

	configUninit();

	if (do_daemonize) {
		removePidfile(pidFileName);
	}
	return 0;
}

