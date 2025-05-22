#include "str.h"
#include <arpa/inet.h>
#include <assert.h>
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
#define QUEUE_SIZE 10
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 1024
#endif

typedef struct {
  char buffer[BUF_SIZE];
  int start;
  int end;
} Buffer;

const int NEXT_BYTE_CLIENT_LEFT = 256;
const int NEXT_BYTE_ERROR = 257;

int next_byte(int client_socket, Buffer *buf) {
  if (buf->start == buf->end) {
    LOG("%s", "refilling buffer");
    int recv_amt = recv(client_socket, buf->buffer, BUF_SIZE, 0);
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

void echo_loop(int client_socket) {
  Buffer buf = {0};

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

    if (send(client_socket, size, 4, 0) == -1) {
      perror("send");
      goto fail;
    }
    if (send(client_socket, msg.str, msg.len, 0) == -1) {
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
}

void accept_loop(int accept_socket) {
  while (true) {
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    LOG("%s", "Waiting for clients...");
    int client_socket = accept(accept_socket, (struct sockaddr *)&client_addr,
                               &client_addr_len);
    LOG("%s", "New connection");
    if (client_socket == -1) {
      perror("Couldnt accept");
      continue;
    }

    if (fork() == 0) {
      echo_loop(client_socket);
      exit(0);
    }
    LOG("%s", "Client finished");
  }
}

int main(int argc, char *argv[]) {
  struct addrinfo hints = {0};
  const char *port = "8080";
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
  accept_loop(accept_socket);

  shutdown(accept_socket, SHUT_RDWR);
  close(accept_socket);

  return 0;
}
