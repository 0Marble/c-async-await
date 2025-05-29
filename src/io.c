#include "io.h"
#include "async.h"
#include "dbg.h"
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
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
  int status = -1;
  while (true) {
    status = recv(fd, buf, n, flags);
    if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      async_skip();
    } else {
      break;
    }
  }
  return status;
}

int await_async_send(int fd, char *buf, int n, int flags) {
  int status = -1;
  while (true) {
    status = send(fd, buf, n, flags);
    if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      async_skip();
    } else {
      break;
    }
  }
  return status;
}

int await_async_accept(int fd, struct sockaddr *addr, socklen_t *addr_len) {
  int status = -1;
  while (true) {
    status = accept(fd, addr, addr_len);
    if (status == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      async_skip();
    } else {
      break;
    }
  }
  return status;
}

int pack(void *buf, int size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  assert(size >= 2 * sizeof(int));
  int ptr = 2 * sizeof(int);

  while (true) {
    char c = *fmt;
    if (c == 0)
      break;
    fmt++;

    switch (c) {
    case 'i': {
      int32_t n = va_arg(ap, int32_t);
      assert(size >= ptr);
      memcpy(buf + ptr, &n, sizeof(int32_t));
      ptr += sizeof(int32_t);
    } break;
    case 'u': {
      uint32_t n = va_arg(ap, uint32_t);
      assert(size >= ptr);
      memcpy(buf + ptr, &n, sizeof(uint32_t));
      ptr += sizeof(uint32_t);
    } break;
    case 'p': {
      long n = (long)va_arg(ap, void *);
      assert(size >= ptr);
      memcpy(buf + ptr, &n, sizeof(long));
      ptr += sizeof(void *);
    } break;

    default: {
      DBG("Invalid type: %c", c);
      return -1;
    } break;
    }
  }
  memcpy(buf, &ptr, sizeof(ptr));
  memcpy(buf + sizeof(int), &size, sizeof(size));
  return ptr;
}

int unpack(void *buf, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = *(int *)buf;
  int size = *(int *)(buf + sizeof(int));
  int ptr = 2 * sizeof(int);

  while (true) {
    char c = *fmt;
    if (c == 0)
      break;
    fmt++;
    assert(len > ptr);

    switch (c) {
    case 'i': {
      int32_t *n = va_arg(ap, int32_t *);
      memcpy(n, buf + ptr, sizeof(int32_t));
      ptr += sizeof(int32_t);
    } break;
    case 'u': {
      uint32_t *n = va_arg(ap, uint32_t *);
      memcpy(n, buf + ptr, sizeof(uint32_t));
      ptr += sizeof(uint32_t);
    } break;
    case 'p': {
      void **n = va_arg(ap, void **);
      memcpy(n, buf + ptr, sizeof(void *));
      ptr += sizeof(void *);
    } break;

    default: {
      DBG("Invalid type: %c", c);
      return -1;
    } break;
    }
  }
  assert(ptr == len);
  return size;
}
