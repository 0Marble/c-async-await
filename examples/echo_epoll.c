#include "str.h"
#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
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
#define QUEUE_SIZE 1000
#endif

#ifndef BUF_SIZE
#define BUF_SIZE 256
#endif

#define EVENTS_CNT 20
#define TIMEOUT 1000

static int bytes_processed = 0;

typedef enum { Reading, Writing } ClientMode;

typedef struct Client {
  ClientMode mode;
  int socket;
  String msg;
  char size[4];
  char read_buffer[BUF_SIZE];
  int read_ptr;
  int read_end;
  int size_ptr;
  int msg_ptr;
  struct Client *prev;
  struct Client *next;
} Client;

typedef struct {
  int accept_socket;
  int epoll_fd;
  struct epoll_event epoll_events[EVENTS_CNT];

  Client *clients;
  Client *free_clients;

} Server;

Client *server_new_client(Server *server, int socket) {
  Client *client = NULL;
  if (server->free_clients) {
    client = server->free_clients;
    server->free_clients = client->next;
    assert(client->prev == NULL);
  } else {
    client = calloc(1, sizeof(Client));
  }
  *client = (Client){0};
  string_init(&client->msg);

  Client *next = server->clients;
  server->clients = client;
  client->next = next;
  if (next) {
    assert(next->prev == NULL);
    next->prev = client;
  }
  client->socket = socket;
  client->mode = Reading;

  return client;
}

void server_close_client(Server *server, Client *client) {
  shutdown(client->socket, SHUT_RDWR);
  close(client->socket);
  string_deinit(&client->msg);

  Client *prev = client->prev;
  Client *next = client->next;
  if (prev) {
    prev->next = next;
  }
  if (next) {
    next->prev = prev;
  }
  client->next = server->free_clients;
  client->prev = NULL;
  client->socket = 0;
}

Server start_server(const char *ip, const char *port) {
  Server server = {0};
  struct addrinfo hints = {0};
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  struct addrinfo *server_info = NULL;

  int gai_status = getaddrinfo(ip, port, &hints, &server_info);
  if (gai_status != 0) {
    LOG("getaddrinfo: %s", gai_strerror(gai_status));
    exit(1);
  }

  server.accept_socket = -1;
  for (struct addrinfo *info = server_info; info; info = info->ai_next) {
    server.accept_socket =
        socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (server.accept_socket == -1)
      continue;

    if (bind(server.accept_socket, info->ai_addr, info->ai_addrlen) == 0) {
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
    close(server.accept_socket);
    server.accept_socket = -1;
  }
  freeaddrinfo(server_info);
  if (server.accept_socket == -1) {
    perror("accept");
    exit(1);
  }
  if (listen(server.accept_socket, QUEUE_SIZE) == -1) {
    perror("listen");
    exit(1);
  }
  Client *dummy_client = server_new_client(&server, server.accept_socket);

  server.epoll_fd = epoll_create1(0);
  struct epoll_event accept_event = (struct epoll_event){
      .events = EPOLLIN,
      .data = (epoll_data_t){.ptr = dummy_client},
  };
  if (epoll_ctl(server.epoll_fd, EPOLL_CTL_ADD, server.accept_socket,
                &accept_event) == -1) {
    perror("epoll_ctl");
    exit(1);
  }

  return server;
}

void stop_server(Server *s) {
  close(s->epoll_fd);

  for (Client *client = s->clients; client; client = client->next) {
    server_close_client(s, client);
  }
  for (Client *client = s->free_clients; client;) {
    Client *next = client->next;
    free(client);
    client = next;
  }
}

int client_on_update(Client *client, uint32_t events) {
  if (client->mode == Reading && events & EPOLLIN) {

    if (client->read_ptr == 0) {
      int status = recv(client->socket, client->read_buffer,
                        sizeof(client->read_buffer), 0);
      if (status == -1) {
        perror("recv");
        return -1;
      }
      if (status == 0) {
        return -1;
      }
      client->read_end = status;
      LOG("read %d bytes", status);
    }

    for (; client->size_ptr < 4; client->size_ptr++) {
      if (client->read_ptr >= client->read_end) {
        client->read_ptr = 0;
        client->read_end = 0;
        return 0;
      }
      client->size[client->size_ptr] = client->read_buffer[client->read_ptr++];
    }

    int msg_len = ntohl(*(uint32_t *)client->size);

    LOG("msg progress=%d/%d", client->msg.len, msg_len);
    for (; client->msg_ptr < msg_len; client->msg_ptr++) {
      if (client->read_ptr >= client->read_end) {
        client->read_ptr = 0;
        client->read_end = 0;
        return 0;
      }
      string_append(&client->msg, client->read_buffer[client->read_ptr++]);
    }
    LOG("got message `%s'", client->msg.str);
    client->msg_ptr = 0;
    client->size_ptr = 0;
    client->mode = Writing;
    bytes_processed += client->msg.len + 4;
  }

  if (client->mode == Writing && events & EPOLLOUT) {
    if (client->size_ptr < 4) {
      char *buf = &client->size[client->size_ptr];
      int buf_size = 4 - client->size_ptr;
      int status = send(client->socket, buf, buf_size, MSG_NOSIGNAL);
      if (status == -1) {
        perror("send size");
        return -1;
      }
      client->size_ptr += status;
    } else if (client->msg_ptr < client->msg.len) {
      char *buf = &client->msg.str[client->msg_ptr];
      int buf_size = client->msg.len - client->msg_ptr;
      int status = send(client->socket, buf, buf_size, MSG_NOSIGNAL);
      if (status == -1) {
        perror("send msg");
        return -1;
      }
      client->msg_ptr += status;
    } else {
      string_clear(&client->msg);
      client->msg_ptr = 0;
      client->size_ptr = 0;
      client->mode = Reading;
    }
  }

  return 0;
}

void run_server(Server *s) {
  LOG("%s", "Running...");
  int current_client_cnt = 0;
  long server_start_time = time(NULL);
  long last_message_time = server_start_time;

  while (true) {
    long current_time = time(NULL);
    if (current_time - last_message_time >= 1) {
      fprintf(stdout, "%ld, %d, %d\n", current_time - server_start_time,
              current_client_cnt, bytes_processed);
      fflush(stdout);
      bytes_processed = 0;
      last_message_time = current_time;
    }

    int cnt = epoll_wait(s->epoll_fd, s->epoll_events, EVENTS_CNT, TIMEOUT);

    if (cnt == -1) {
      perror("epoll_wait");
      continue;
    }

    for (int i = 0; i < cnt; i++) {
      Client *client = s->epoll_events[i].data.ptr;

      if (client->socket == s->accept_socket) {
        current_client_cnt++;

        struct sockaddr_storage client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        LOG("%s", "Waiting for clients...");
        int client_socket =
            accept(s->accept_socket, (struct sockaddr *)&client_addr,
                   &client_addr_len);
        LOG("%s", "New connection");
        if (client_socket == -1) {
          perror("Couldnt accept");
          continue;
        }

        struct epoll_event ev = {
            .events = EPOLLIN | EPOLLOUT,
            .data = server_new_client(s, client_socket),
        };
        if (epoll_ctl(s->epoll_fd, EPOLL_CTL_ADD, client_socket, &ev) == -1) {
          perror("epoll_ctl");
          continue;
        }
      } else {
        assert(client->socket != 0);
        uint32_t events = s->epoll_events[i].events;
        int status = client_on_update(client, events);
        if (status == -1 || events & EPOLLHUP || events & EPOLLERR) {
          server_close_client(s, client);
          LOG("%s", "client left");
          current_client_cnt--;
        }
      }
    }
  }
}

int main(int argc, char *argv[]) {
  Server s = start_server(NULL, (argc == 1 ? "8080" : argv[1]));
  run_server(&s);
  stop_server(&s);

  return 0;
}
