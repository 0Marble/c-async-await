#include "../src/async.h"
//
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void *get_in_addr(struct sockaddr *sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in *)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

void async_send(void *data) {
  int fd = 0;
  void *buf = NULL;
  int size = 0;
  int data_buf_size = unpack(data, "ipi", &fd, &buf, &size);
  while (true) {
    int res = send(fd, buf, size, 0);
    if (res == -1) {
      if (errno == EAGAIN) {
        async_skip();
        continue;
      }
      perror("send");
      async_return(NULL);
    }

    pack(data, data_buf_size, "i", res);
    break;
  }
  async_return(data);
}

void async_recv(void *data) {
  int fd = 0;
  void *buf = NULL;
  int size = 0;
  int data_buf_size = unpack(data, "ipi", &fd, &buf, &size);
  while (true) {
    int status = recv(fd, buf, size, 0);
    if (status == -1) {
      if (errno == EAGAIN) {
        async_skip();
        continue;
      }
      perror("send");
      async_return(NULL);
    }

    pack(data, data_buf_size, "i", status);
    break;
  }
  async_return(data);
}

void async_request(void *data) {
  const char *addr = NULL;
  const char *port = NULL;
  const char *path = NULL;
  int data_size = unpack(data, "ppp", &addr, &port, &path);

  char name[1024] = {0};
  assert(snprintf(name, sizeof(name), "%s:%s/%s", addr, port, path) > 0);
  printf("[%s] started...\n", name);

  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *server_info = NULL;
  struct addrinfo *final_addr = NULL;
  char server_ip[INET6_ADDRSTRLEN] = {0};

  int rv = getaddrinfo(addr, port, &hints, &server_info);
  if (rv != 0) {
    fprintf(stderr, "[%s] getaddrinfo: %s\n", name, gai_strerror(rv));
    async_return((void *)0);
  }

  int sock = -1;
  printf("[%s] trying to connect...\n", name);
  for (struct addrinfo *p = server_info; p; p = p->ai_next) {
    sock = socket(p->ai_family, p->ai_socktype | O_NONBLOCK, p->ai_protocol);
    if (sock == -1) {
      continue;
    }

    while (true) {
      int status = connect(sock, p->ai_addr, p->ai_addrlen);
      if (status != -1)
        break;
      if (errno == EAGAIN || errno == EINPROGRESS) {
        async_skip();
        continue;
      } else
        goto cant_connect;
    }

    final_addr = p;
    break;

  cant_connect:
    close(sock);
  }

  if (sock == -1) {
    fprintf(stderr, "[%s] Couldnt connect\n", name);
    freeaddrinfo(server_info);
    async_return((void *)0);
  }

  inet_ntop(final_addr->ai_family,
            get_in_addr((struct sockaddr *)final_addr->ai_addr), server_ip,
            sizeof(server_ip));
  printf("[%s] connected to %s\n", name, server_ip);

  freeaddrinfo(server_info);

  const char *fmt = "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n";
  char buffer[1024] = {0};
  int amt = snprintf(buffer, sizeof(buffer), fmt, path, addr);
  if (amt < 0 || amt > sizeof(buffer)) {
    perror("snprintf:");
    async_return((void *)0);
  }

  char arg_buf[256] = {0};
  int arg_ptr = 0;
  long buffer_ptr = (long)(void *)buffer;
  pack(arg_buf, sizeof(arg_buf), "ipi", sock, &buffer, amt);
  Handle h = async_call(async_send, arg_buf);
  await(h);
  async_free(h);
  arg_ptr = 0;

  printf("[%s] Waiting for response\n", name);
  uint8_t sum = 0;

  while (true) {
    memset(buffer, 0, sizeof(buffer));

    int amt = 0;
    pack(arg_buf, sizeof(arg_buf), "ipi", sock, buffer, 256);
    Handle h = async_call(async_recv, arg_buf);
    unpack(await(h), "i", &amt);
    async_free(h);
    printf("[%s] got %d bytes...\n", name, amt);

    if (amt < 0) {
      perror("recv:");
      break;
    }
    for (int j = 0; j < amt; j++) {
      sum += buffer[j];
    }
    if (amt == 0)
      break;
  }

  close(sock);
  pack(data, data_size, "u", sum);
  async_return(data);
}

void async_main(void *data) {
  const char *addr = "0.0.0.0";
  const char *port = "8000";

  int cnt = 6;
  const char *paths[6] = {"",
                          "build.zig",
                          "build.zig.zon",
                          "src/client.zig",
                          "src/server.zig",
                          "stress.py"};
  char args[6][256] = {0};
  Handle hs[6] = {0};

  for (int i = 0; i < cnt; i++) {
    pack(args[i], sizeof(args[i]), "ppp", addr, port, paths[i]);
    hs[i] = async_call(async_request, args[i]);
  }
  await_all(hs, cnt, NULL);

  for (int i = 0; i < cnt; i++) {
    uint32_t sum = 0;
    unpack(args[i], "u", &sum);
    printf("[%s:%s/%s] sum=%d\n", addr, port, paths[i], sum);
  }

  async_return(NULL);
}

int main(int argc, char **argv) {
  run_async_main(async_main, NULL);
  return 0;
}
