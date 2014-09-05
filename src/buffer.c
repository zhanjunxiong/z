#include "buffer.h"

#include "zmalloc.h"

#include <assert.h>
#include <string.h>

struct buffer {
	uint32_t writeIndex;
	uint32_t readIndex;
	uint32_t initSize;
	uint32_t size;
	char *data;
};

static const uint32_t kCheapPrepend = 0;
struct buffer* bufferCreate(uint32_t initSize) {
	struct buffer* buf = (struct buffer*)zcalloc(sizeof(*buf));
	buf->data = NULL;
	buf->readIndex = kCheapPrepend;
	buf->writeIndex = kCheapPrepend;
	buf->size = initSize;
	buf->initSize = initSize;
	if (initSize == 0) {
		return buf;
	}

	buf->data = (char*)zmalloc(kCheapPrepend + initSize);
	return buf;
}

void bufferReset(struct buffer* buf) {
	assert(buf != NULL);

	buf->size = buf->initSize;
	buf->data = (char*)zmalloc(kCheapPrepend + buf->initSize);
	buf->readIndex = kCheapPrepend;
	buf->writeIndex = kCheapPrepend;
}

void bufferRelease(struct buffer* buf) {
	if (buf->data) {
		zfree(buf->data);
	}
	zfree(buf);
}

char* bufferWriteIndex(struct buffer* buf) {
	return bufferBegin(buf) + buf->writeIndex;
}

char* bufferReadIndex(struct buffer* buf) {
	return bufferBegin(buf) + buf->readIndex;
}

void bufferMoveWriteIndex(struct buffer* buf, uint32_t moveLen) {
	buf->writeIndex += moveLen;
}

void bufferMoveReadIndex(struct buffer* buf, uint32_t moveLen) {
	if (moveLen < bufferReadableBytes(buf)) {
		buf->readIndex += moveLen;
	} else {
		bufferRetrieveAll(buf);
	}
}

char* bufferBegin(struct buffer* buf) {
	return buf->data;
}

void bufferAppend(struct buffer* buf, const char* data, uint32_t dataLen) {
	if (dataLen > bufferWriteableBytes(buf)) {
		bufferMakeSpace(buf, dataLen);
	}

	memcpy(bufferBegin(buf) + buf->writeIndex, data, dataLen);
	buf->writeIndex += dataLen;
}

void bufferPrepend(struct buffer* buf, const char* data, uint32_t dataLen) {
	if (dataLen <= bufferPrependableBytes(buf)) {
		return;
	}

	uint32_t readIndex = buf->readIndex;
	readIndex -= dataLen;
	memcpy(bufferBegin(buf) + readIndex, data, dataLen);
	buf->readIndex = readIndex;
}

void bufferRetrieveAll(struct buffer* buf) {
	buf->writeIndex = kCheapPrepend;
	buf->readIndex = kCheapPrepend;
}


//
void bufferMakeSpace(struct buffer* buf, uint32_t more) {
	if (!buf->data) {
		buf->data = (char*)zmalloc(more + kCheapPrepend);
		buf->writeIndex = kCheapPrepend;
		buf->readIndex = kCheapPrepend;
	} else if (bufferWriteableBytes(buf) + bufferPrependableBytes(buf)  < more + kCheapPrepend) {
		bufferExpand(buf);
	} else {
		uint32_t used = bufferReadableBytes(buf);
		memmove(bufferBegin(buf) + kCheapPrepend, bufferReadIndex(buf), used);
		buf->readIndex = kCheapPrepend;
		buf->writeIndex = buf->readIndex + used;
	}
}

uint32_t bufferExpand(struct buffer* buf) {
	int reallocSize = buf->size << 1;
	char* tmp = (char*)zrealloc(buf->data, reallocSize * sizeof(char));
	buf->size = reallocSize;
	buf->data = tmp;
	return 0;
}

uint32_t bufferWriteableBytes(struct buffer* buf) {
	return buf->size - buf->writeIndex;
}

uint32_t bufferReadableBytes(struct buffer* buf) {
	return buf->writeIndex - buf->readIndex;
}

uint32_t bufferPrependableBytes(struct buffer* buf) {
	return buf->readIndex;
}

uint32_t bufferGetSize(struct buffer* buf) {
	return buf->size;
}

void bufferSetWriteIndex(struct buffer* buf, uint32_t index) {
	buf->writeIndex = index;
}

char* bufferPeek(struct buffer* buf) {
	return bufferBegin(buf) + buf->readIndex;
}

int32_t bufferPeekInt32(struct buffer* buf) {
	int32_t be32 = 0;
	memcpy(&be32, bufferPeek(buf), sizeof(be32));
	return be32;
}

int16_t bufferPeekInt16(struct buffer* buf) {
	int16_t be16 = 0;
	memcpy(&be16, bufferPeek(buf), sizeof(be16));
	return be16;
}

int32_t bufferReadInt32(struct buffer* buf) {
	int32_t ret = bufferPeekInt32(buf);
	bufferMoveReadIndex(buf, sizeof(int32_t));
	return ret;
}

