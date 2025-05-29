#ifndef __IO_H__
#define __IO_H__

#include "async.h"
#include <sys/socket.h>

Handle async_recv(int fd, char *buf, int n, int flags);
Handle async_send(int fd, char *buf, int n, int flags);
Handle async_accept(int fd, struct sockaddr *addr, socklen_t *addr_len);

int await_async_recv(int fd, char *buf, int n, int flags);
int await_async_send(int fd, char *buf, int n, int flags);
int await_async_accept(int fd, struct sockaddr *addr, socklen_t *addr_len);

int pack(void *buf, int size, const char *fmt, ...);
int unpack(void *buf, const char *fmt, ...);

#endif
