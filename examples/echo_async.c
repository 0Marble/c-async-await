#include "../src/async.h"
#include "../src/io.h"

#include "str.h"
#include <arpa/inet.h>
#include <assert.h>
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
#include <time.h>
#include <unistd.h>

#ifndef NOLOG
#define LOG(fmt, ...)                                                          \
  fprintf(stderr, "[%s:%d] " fmt "\n", __FILE_NAME__, __LINE__, __VA_ARGS__)
#else
#define LOG(fmt, ...)
#endif

#ifndef QUEUE_SIZE
#define QUEUE_SIZE 100
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 256
#endif

#ifndef SLEEP_TIME
#define SLEEP_TIME 10000
#endif

static int current_client_cnt = 0;
static int max_client_cnt = 0;
static int bytes_processed = 0;

typedef struct {
  char buffer[BUF_SIZE];
  int start;
  int end;
} Buffer;

int read_n_bytes(int sock, int n, char *out, Buffer *buf) {
  int amt = 0;
  while (amt < n) {
    if (buf->start == buf->end) {
      int status = await_async_recv(sock, buf->buffer, sizeof(buf->buffer), 0);
      if (status == -1) {
        perror("recv");
        return -1;
      } else if (status == 0) {
        return 0;
      }
      buf->start = 0;
      buf->end = status;
    }
    assert(buf->start < buf->end);

    int write_amt = n - amt;
    if (write_amt > buf->end - buf->start) {
      write_amt = buf->end - buf->start;
    }
    memcpy(out + amt, buf->buffer + buf->start, write_amt);
    buf->start += write_amt;
    amt += write_amt;
  }

  return amt;
}

int write_n_bytes(int sock, int n, char *msg) {
  int sent_amt = 0;
  while (sent_amt < n) {
    int status = await_async_send(sock, msg + sent_amt, n - sent_amt, 0);
    if (status == -1) {
      perror("send");
      return -1;
    }
    sent_amt += status;
  }
  return sent_amt;
}

void echo_loop(void *args) {
  int client_socket = (int)(long)args;
  Buffer buf = {0};

  String msg;
  string_init(&msg);
  bool running = true;
  current_client_cnt++;
  if (current_client_cnt > max_client_cnt) {
    max_client_cnt = current_client_cnt;
  }

  while (running) {
    char size[4] = {0};
    if (read_n_bytes(client_socket, 4, size, &buf) != 4) {
      goto fail;
    }

    int msg_len = ntohl(*(uint32_t *)size);
    LOG("message length: %d", msg_len);
    bytes_processed += msg_len;
    string_clear(&msg);
    string_resize(&msg, msg_len, '!');
    if (read_n_bytes(client_socket, msg_len, msg.str, &buf) != msg_len) {
      goto fail;
    }

    LOG("got message: `%s'", msg.str);
    if (write_n_bytes(client_socket, 4, size) != 4) {
      goto fail;
    }
    if (write_n_bytes(client_socket, msg_len, msg.str) != msg_len) {
      goto fail;
    }
  }
fail:
  LOG("%s", "Client left");

  current_client_cnt--;

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
    int client_socket = await_async_accept(
        accept_socket, (struct sockaddr *)&client_addr, &client_addr_len);
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
    async_orphan(h);
  }
}

void server_stats(void *args) {
  long server_start_time = time(NULL);
  long last_message_time = server_start_time;

  while (true) {
    long current_time = time(NULL);
    if (current_time - last_message_time >= 1) {
      fprintf(stdout, "%ld, %d, %d\n", current_time - server_start_time,
              max_client_cnt, bytes_processed);
      fflush(stdout);
      bytes_processed = 0;
      last_message_time = current_time;
    }
    struct timespec sleep_delay = {
        .tv_sec = 0,
        .tv_nsec = SLEEP_TIME,
    };
    if (nanosleep(&sleep_delay, NULL) < 0) {
      perror("nanosleep:");
    }
    async_skip();
  }
  async_return(NULL);
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
  async_orphan(async_call(server_stats, NULL));
  await(async_call(accept_loop, &accept_socket));

  shutdown(accept_socket, SHUT_RDWR);
  close(accept_socket);

  async_return(NULL);
}

int main(int argc, char *argv[]) {
  run_async_main(async_main, (argc == 1 ? "8080" : argv[1]));
}
