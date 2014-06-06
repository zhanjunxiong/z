
#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdint.h>
#include <stdlib.h>

struct buffer;

struct buffer* bufferCreate(uint32_t initSize);
void bufferRelease(struct buffer*);

char* bufferWriteIndex(struct buffer*);
char* bufferReadIndex(struct buffer*);

void bufferMoveWriteIndex(struct buffer*, uint32_t);
void bufferMoveReadIndex(struct buffer*, uint32_t);

void bufferAppend(struct buffer*, const char*, uint32_t);
void bufferPrepend(struct buffer*, const char*, uint32_t);

void bufferRetrieveAll(struct buffer* buf);

char* bufferBegin(struct buffer* buf);

uint32_t bufferExpand(struct buffer* buf);
uint32_t bufferWriteableBytes(struct buffer*);
uint32_t bufferReadableBytes(struct buffer*);
uint32_t bufferPrependableBytes(struct buffer*);

void bufferMakeSpace(struct buffer*, uint32_t);

uint32_t bufferGetSize(struct buffer* buf); 

void bufferSetWriteIndex(struct buffer* buf, uint32_t index); 

int32_t bufferPeekInt32(struct buffer* buf);
int16_t bufferPeekInt16(struct buffer* buf);
char* bufferPeek(struct buffer* buf);

int32_t bufferReadInt32(struct buffer* buf);

#endif 

