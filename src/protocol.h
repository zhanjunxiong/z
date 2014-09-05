#ifndef PROTOCOL_H_
#define PROTOCOL_H_

void defaultMessageCallback(struct session *session, struct buffer *buf);

void defaultConnectionCallback(struct session *session);
void defaultCloseCallback(struct session *session);
void defaultWriteCompleteCallback(struct session *session);
void defaultAcceptCallback(struct session *session, struct session *newSession);

#endif


