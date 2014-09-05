
#include "z.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <time.h>
#include <sys/time.h>

struct logger {
	FILE* handle;
	char name[FILENAME_MAX];
	char rotateDir[FILENAME_MAX];
	int close;
	int maxLineNum;
	int lineNum;
};

static const char* sLevelMsg[] = { 
								"ERROR",
								"WARN",
								"LOG",
								"MSG",
								"DEBUG",
								"STAT",
								};

static void _rotateFile(struct logger* inst, struct tm* tm) {
	if (inst->close == 0) return;

	if (inst->lineNum++ > inst->maxLineNum) {
		fclose(inst->handle);

		char filePath[FILENAME_MAX];
		memset(filePath, 0, FILENAME_MAX);
		snprintf(filePath, FILENAME_MAX - 1, "%s/%s_%04d_%02d_%02d_%02d_%02d_%02d",
							inst->rotateDir,
							inst->name,
							tm->tm_year + 1900,
							tm->tm_mon + 1,
							tm->tm_mday,
							tm->tm_hour,
							tm->tm_min,
							tm->tm_sec);
		
		/*
		fprintf(stderr, "filepath: %s, rotatedir: %s, name: %s\n", 
				filePath,
				inst->rotateDir,
				inst->name);
		*/

		rename(inst->name, filePath);
		
		inst->lineNum = 0;
		inst->handle = fopen(inst->name,"w+");
		if (inst->handle == NULL) {
			fprintf(stderr, "rotate file faile, open filename(%s) fail\n", inst->name);
			return;
		}
	}
}

static int _logger(struct context* ctx, 
					void* ud, 
					struct message* msg) {

	int type = msg->type & 0x0000FFFF;
	assert(type >= MSG_TYPE_LOG_ERROR && type < MSG_TYPE_LOG_LEN);

	struct logger* inst = (struct logger*)ud;

	struct timeval t;
	gettimeofday(&t, NULL);
	struct tm* tm = localtime(&t.tv_sec);
	_rotateFile(inst, tm); 

	fprintf(inst->handle, "[%04d-%02d-%02d %02d:%02d:%02d:%06ld][%d][%s]",
						tm->tm_year+1900,
						tm->tm_mon+1,
						tm->tm_mday,
						tm->tm_hour,
						tm->tm_min,
						tm->tm_sec,
						t.tv_usec,
						msg->from,
						sLevelMsg[type]);

	fwrite(msg->data, msg->sz, 1, inst->handle);
	fprintf(inst->handle, "\n");
	fflush(inst->handle);

	return 0;
}

extern "C" struct logger* loggerCreate() {
	struct logger* inst = (struct logger*)zcalloc(sizeof(*inst));
	inst->handle = NULL;
	inst->close = 0;
	inst->lineNum = 0;
	return inst;
}

extern "C" int loggerInit(struct logger* inst, struct context* ctx, const char* parm) {

	if (parm && strcmp(parm, "") != 0) {
		char p[strlen(parm) + 1];
		memset(p, 0, strlen(parm) + 1);
		memcpy(p, parm, strlen(parm));

		char* tmp = p;
		const char* name = strsep(&tmp, " \t\r\n");
		if (name == NULL) {
			fprintf(stderr, "file name is null\n");
			return 1;
		}
		int maxLineNum = 200000;
		const char* str = strsep(&tmp, " \t\r\n");
		if (str) {
			maxLineNum = atoi(str);

			const char* rotateDir = strsep(&tmp, " \t\r\n");
			if (rotateDir == NULL) {
				rotateDir = "";
			} else {
				memset(inst->rotateDir, '\0', FILENAME_MAX);
				memcpy(inst->rotateDir, rotateDir, strlen(rotateDir));
			}
		}

		memset(inst->name, 0, FILENAME_MAX);

		memcpy(inst->name, name, strlen(name));
		inst->maxLineNum = maxLineNum;
		inst->handle = fopen(name,"w");
		if (inst->handle == NULL) {
			return 1;
		}
		inst->close = 1;

		/*	
		fprintf(stderr, "filename: %s, maxlinenum: %d, rotateDir: %s\n", 
						name,
						inst->maxLineNum,
						inst->rotateDir);
		
		*/
	} else {
		inst->handle = stdout;
	}

	if (inst->handle) {
		zSetCallback(ctx, _logger, inst);
		return 0;
	}
	return 1;
}

extern "C" void loggerRelease(struct logger* inst) {
	if (inst->close) {
		fclose(inst->handle);
	}
	zfree(inst);
}

