#include "io.h"
#include "async.h"
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/socket.h>

void async_recv_impl(void *args) {
  int fd = 0, n = 0, flags = 0;
  char *buf = NULL;
  unpack(args, "ipii", &fd, &buf, &n, &flags);

  int status = -1;
  while (true) {
    status = recv(fd, buf, n, flags);
    if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      async_skip();
    } else {
      break;
    }
  }

  async_return((void *)(long)status);
}

Handle async_recv(int fd, char *buf, int n, int flags) {
  char arg_buf[256] = {0};
  pack(arg_buf, sizeof(arg_buf), "ipii", fd, buf, n, flags);
  Handle h = async_call(async_recv_impl, arg_buf);
  return h;
}

void async_send_impl(void *args) {
  int fd = 0, n = 0, flags = 0;
  char *buf = NULL;
  unpack(args, "ipii", &fd, &buf, &n, &flags);

  int status = -1;
  while (true) {
    status = send(fd, buf, n, flags);
    if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      async_skip();
    } else {
      break;
    }
  }

  async_return((void *)(long)status);
}

Handle async_send(int fd, char *buf, int n, int flags) {
  char arg_buf[256] = {0};
  pack(arg_buf, sizeof(arg_buf), "ipii", fd, buf, n, flags);
  Handle h = async_call(async_send_impl, arg_buf);
  return h;
}

void async_accept_impl(void *args) {
  int fd = 0;
  struct sockaddr *addr = NULL;
  socklen_t *addr_len = NULL;
  unpack(args, "ipp", &fd, &addr, &addr_len);

  int status = -1;
  while (true) {
    status = accept(fd, addr, addr_len);
    if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      async_skip();
    } else {
      break;
    }
  }

  async_return((void *)(long)status);
}

Handle async_accept(int fd, struct sockaddr *addr, socklen_t *addr_len) {
  char arg_buf[256] = {0};
  pack(arg_buf, sizeof(arg_buf), "ipp", fd, addr, addr_len);
  Handle h = async_call(async_accept_impl, arg_buf);
  return h;
}

int await_async_recv(int fd, char *buf, int n, int flags) {
  Handle h = async_recv(fd, buf, n, flags);
  int res = (int)(long)await(h);
  async_free(h);
  return res;
}

int await_async_send(int fd, char *buf, int n, int flags) {
  Handle h = async_send(fd, buf, n, flags);
  int res = (int)(long)await(h);
  async_free(h);
  return res;
}

int await_async_accept(int fd, struct sockaddr *addr, socklen_t *addr_len) {
  Handle h = async_accept(fd, addr, addr_len);
  int res = (int)(long)await(h);
  async_free(h);
  return res;
}
