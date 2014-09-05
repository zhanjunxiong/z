
#include "ae.h"
#include "env.h"
#include "ctx_mgr.h"
#include "daemon.h"
#include "eventloop.h"
#include "blockqueue.h"
#include "module.h"
#include "name_mgr.h"
#include "queue.h"
#include "context.h"
#include "log.h"
#include "event_msgqueue.h"
#include "globalqueue.h"
#include "thread.h"
#include "tcp.h"
#include "gen_tcp.h"
#include "timer.h"
#include "zmalloc.h"
#include "worker_pool.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

static void unSetupSignalHandlers() {
	struct sigaction act;
	
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	act.sa_handler = SIG_IGN;
	sigaction(SIGTERM, &act, NULL);
	sigaction(SIGINT, &act, NULL);
}


static void stop() {
	unSetupSignalHandlers();

	timerStop();
	ctxMgrStop();
	workerPoolStop();
	tcpStop();
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
	sigaction(SIGINT, &act, NULL);

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

static void start() {

	timerStart();
	workerPoolStart();
	eventloopStart();

	// 1. setup signal
	setupSignalHandlers();


	timerJoin();
	workerPoolJoin();
	eventloopJoin();
	logExit();
}

static void showVersion() {
	printf("z version is %.02f\n", Z_VERSION);
	exit(1);
}

static void showHelp() {
	printf("please use 'z help' instead!\n");
	exit(1);
}

static void savePid(const pid_t pid, const char *programName) {
	FILE *fp;
	if (programName == NULL)
		return;

	char filename[FILENAME_MAX];
	strncat(filename, Z_PID_PATH, strlen(Z_PID_PATH));
	strncat(filename, programName, strlen(programName));
	strncat(filename, ".pid", strlen(".pid"));

	if ((fp = fopen(filename, "w+")) == NULL) {
		logVperror("Could not open the pid file %s for writing", filename);
		exit(0);
	}

	fprintf(fp,"%ld\n", (long)pid);
	if (fclose(fp) == -1) {
		logVperror("Could not close the pid file %s", filename);
		return;
	}
}

static void removePidfile(const char *programName) {
	if (programName == NULL)
		return;

	char filename[FILENAME_MAX];
	strncat(filename, Z_PID_PATH, strlen(Z_PID_PATH));
	strncat(filename, programName, strlen(programName));
	strncat(filename, ".pid", strlen(".pid"));

	if (unlink(filename) != 0) {
		logVperror("Could not remove the pid file %s", filename);
	}
}

static void bootstrap(const char* name) {
	struct context* ctx = ctxMgrCreateContext("zlua", name);
	if (ctx == NULL) {
		logVperror("load (%s) zlua fail\n", name);
		ctxMgrCreateContext("zlua", "LOAD help ");
		exit(1);
	}
}

static uint32_t nextpow2(uint32_t num) {
    --num;
    num |= num >> 1;
    num |= num >> 2;
    num |= num >> 4;
    num |= num >> 8;
    num |= num >> 16;
    return ++num;
}

static int init(const char *programName, bool do_daemonize) {
	const char* modulePath = envGet("modulePath", "./cservice/?.so;");
	int threadNum = envGetInt("thread_num", 4);
	int fdSetSize = envGetInt("fd_size", 1024);
	int ctxNum = envGetInt("ctx_num", 32);

	fdSetSize = nextpow2(fdSetSize);
	assert(logInit(programName, do_daemonize) == 0);
	assert(globalqueueInit() == 0);
	assert(ctxMgrInit(ctxNum) == 0);
	assert(eventloopInit(fdSetSize) == 0);
	assert(tcpInit(fdSetSize) == 0);
	assert(timerInit() == 0);
	assert(moduleInit(modulePath) == 0);
	assert(workerPoolInit(threadNum) == 0);
	assert(nameMgrInit() == 0);
	return 0;
}

static void uninit() {
	tcpUninit();
	eventloopUninit();
	globalqueueUninit();
	timerUninit();
	ctxMgrUninit();
	moduleUninit();
	nameMgrUninit();
	workerPoolUninit();
	logUninit();
}

int main(int argc, char **argv) {

	int o;
	bool do_daemonize = false;
	const char* programName = NULL;
	const char* envFileName = NULL;
	while(-1 != (o = getopt(argc, argv,
					"h" // show help
					"d" // run as a daemon
					"v" // show version
					"P:" // program name
					"e:" // env config file
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
			case 'P':
				programName = optarg;
				break;
			case 'e':
				envFileName = optarg;
				break;
			default:
				showHelp();
				return -1;
		}
	}
	// 0 build bootstrap programe name
	char bootstrapName[1024] = ""; 
	int bootstrapNameSize = 0;
	int bootname = 0;
	int i;
	for (i=0; optind<argc; i++,optind++) {
		bootname = 1;
		int argvLen = strlen(argv[optind]);
		strncat(bootstrapName, argv[optind], argvLen);
		strncat(bootstrapName, " ", 1);
		bootstrapNameSize = bootstrapNameSize + argvLen + 1;
	}
	if (bootname == 0) {
		strncat(bootstrapName, "help", strlen("help"));
		strncat(bootstrapName, " ", 1);
		bootstrapNameSize = bootstrapNameSize + strlen("help") + 1;
	}
	bootstrapName[bootstrapNameSize] = '\0';
	bootstrapNameSize = bootstrapNameSize + 1;
	assert(bootstrapNameSize < 1024);

	// 2. change work dir 
	char* workDir = getenv("Z");
	if (workDir) {
		chdir(workDir);
	}
	// 3. get current work dir
	char path[FILENAME_MAX];
	if(!getcwd(path, FILENAME_MAX)) {
		logVperror("z getcwd error\n"); 
		return -1;
	}

	if (programName == NULL) {
		programName = Z_PROGNAME;
	} 
	// 4. check daemon
	if (do_daemonize) {
		if (daemonize(1, 0) == -1) {
			logVperror("failed to daemon() in order to daemonize\n");
			return -1;
		}

		
		savePid(getpid(), programName);
	}
	// 5. init env
	assert(envInit(envFileName) == 0);

	init(programName, do_daemonize);
	bootstrap(bootstrapName);

	start();
	uninit();

	envUninit();

	if (do_daemonize) {
		removePidfile(programName);
	}
	return 0;
}

