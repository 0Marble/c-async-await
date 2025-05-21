#include "../src/async.h"
#include "str.h"
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef LOG
#define LOG(fmt, ...)                                                          \
  fprintf(stderr, "[%s:%d] " fmt "\n", __FILE_NAME__, __LINE__, __VA_ARGS__)
#endif

#ifndef QUEUE_SIZE
#define QUEUE_SIZE 100
#endif

typedef struct {
  char buffer[1024];
  int start;
  int end;
  int size;
} Buffer;

const int NEXT_BYTE_CLIENT_LEFT = 256;
const int NEXT_BYTE_ERROR = 257;

void async_recv(void *args) {
  int sock = 0;
  char *buf = NULL;
  int buf_size = 0;
  int flags = 0;
  int size = unpack(args, "ipii", &sock, &buf, &buf_size, &flags);

  int res = 0;
  while (true) {
    res = recv(sock, buf, buf_size, flags);
    if (res == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      async_skip();
    } else {
      break;
    }
  }
  async_return(&res);
}

void async_send(void *args) {
  int sock = 0;
  char *buf = NULL;
  int buf_size = 0;
  int flags = 0;
  int size = unpack(args, "ipii", &sock, &buf, &buf_size, &flags);

  int res = 0;
  while (true) {
    res = send(sock, buf, buf_size, flags);
    if (res == -1 && (errno == EWOULDBLOCK || errno == EAGAIN)) {
      async_skip();
    } else {
      break;
    }
  }
  async_return(&res);
}

int await_async_recv(int sock, char *buf, int n, int flags) {
  char args[256] = {0};
  pack(args, sizeof(args), "ipii", sock, buf, n, flags);
  Handle h = async_call(async_recv, args);
  int recv_amt = *(int *)await(h);
  async_free(h);
  return recv_amt;
}

int await_async_send(int sock, char *buf, int n, int flags) {
  char args[256] = {0};
  pack(args, sizeof(args), "ipii", sock, buf, n, flags);
  Handle h = async_call(async_send, args);
  int recv_amt = *(int *)await(h);
  async_free(h);
  return recv_amt;
}

int next_byte(int client_socket, Buffer *buf) {
  if (buf->start == buf->end) {
    LOG("%s", "refilling buffer");

    int recv_amt = await_async_recv(client_socket, buf->buffer, buf->size, 0);
    if (recv_amt == -1) {
      perror("recv");
      return NEXT_BYTE_ERROR;
    } else if (recv_amt == 0) {
      return NEXT_BYTE_CLIENT_LEFT;
    }
    LOG("got %d bytes", recv_amt);
    buf->start = 0;
    buf->end = recv_amt;
  }
  assert(buf->start < buf->end);
  return buf->buffer[buf->start++];
}
#define next_byte_or_break(res, client_socket, buf, if_failed)                 \
  do {                                                                         \
    int x = next_byte(client_socket, buf);                                     \
    if (x >= 256)                                                              \
      goto if_failed;                                                          \
    else                                                                       \
      res = x;                                                                 \
  } while (0)

void echo_loop(void *args) {
  int client_socket = (int)(long)args;
  Buffer buf = {0};
  buf.size = 1024;

  String msg;
  string_init(&msg);
  bool running = true;

  while (running) {
    char size[4] = {0};
    next_byte_or_break(size[0], client_socket, &buf, fail);
    next_byte_or_break(size[1], client_socket, &buf, fail);
    next_byte_or_break(size[2], client_socket, &buf, fail);
    next_byte_or_break(size[3], client_socket, &buf, fail);

    int msg_len = ntohl(*(uint32_t *)size);
    LOG("message length: %d", msg_len);
    for (int i = 0; i < msg_len; i++) {
      char c = '\0';
      next_byte_or_break(c, client_socket, &buf, fail);
      string_append(&msg, c);
    }
    LOG("got message: `%s'", msg.str);

    if (await_async_send(client_socket, size, 4, 0) == -1) {
      perror("send");
      goto fail;
    }
    if (await_async_send(client_socket, msg.str, msg.len, 0) == -1) {
      perror("send");
      goto fail;
    }
    string_clear(&msg);
  }
fail:
  LOG("%s", "Client left");

  string_deinit(&msg);
  shutdown(client_socket, SHUT_RDWR);
  close(client_socket);
  async_return(NULL);
}

void accept_loop(void *args) {
  int accept_socket = *(int *)args;

  while (true) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    LOG("%s", "Waiting for clients...");
    int client_socket = 0;
    while (true) {
      client_socket = accept(accept_socket, (struct sockaddr *)&client_addr,
                             &client_addr_len);
      if (client_socket == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        async_skip();
      } else {
        LOG("%s", "new user");
        break;
      }
    }
    if (client_socket == -1) {
      perror("Couldnt accept");
      continue;
    }
    if (fcntl(client_socket, F_SETFL, O_NONBLOCK) == -1) {
      perror("fcntl");
      continue;
    }
    LOG("%s", "New connection");

    Handle h = async_call(echo_loop, (void *)(long)client_socket);
    LOG("%s", "Client finished");
  }
}

void async_main(void *args) {
  struct addrinfo hints = {0};
  const char *port = args;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *server_info = NULL;

  int gai_status = getaddrinfo(NULL, port, &hints, &server_info);
  if (gai_status != 0) {
    LOG("getaddrinfo: %s", gai_strerror(gai_status));
    exit(1);
  }

  int accept_socket = -1;
  for (struct addrinfo *info = server_info; info; info = info->ai_next) {
    accept_socket =
        socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (accept_socket == -1)
      continue;

    if (bind(accept_socket, info->ai_addr, info->ai_addrlen) == 0) {
      LOG("%s", "Server running on");
      char ip_addr[64] = {0};
      inet_ntop(info->ai_family, info->ai_addr, ip_addr, sizeof(ip_addr));
      if (info->ai_family == AF_INET) {
        LOG("\tIPv4, %s:%s", ip_addr, port);
      } else {
        LOG("\tIPv6, %s:%s", ip_addr, port);
      }
      break;
    }
    close(accept_socket);
    accept_socket = -1;
  }
  freeaddrinfo(server_info);
  if (accept_socket == -1) {
    perror("accept");
    exit(1);
  }
  if (listen(accept_socket, QUEUE_SIZE) == -1) {
    perror("listen");
    exit(1);
  }

  LOG("%s", "Listening...");
  if (fcntl(accept_socket, F_SETFL, O_NONBLOCK) == -1) {
    perror("fcntl");
    exit(1);
  }
  await(async_call(accept_loop, &accept_socket));

  shutdown(accept_socket, SHUT_RDWR);
  close(accept_socket);

  async_return(NULL);
}

int main(int argc, char *argv[]) { run_async_main(async_main, "8080"); }
