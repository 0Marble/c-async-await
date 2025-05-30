#include "scheduler.h"
#include "storage.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef struct {
  Handle *elems;
  int start, end, cap;
} Queue;

void register_task(void *data, Handle h) {
  Queue *q = data;
  queue_push_back(q, h);
}

void finish_task(void *data, Handle *cur, Handle *next) {
  Queue *q = data;
  queue_pop_front(q, cur);
  queue_peek_front(q, next);
}

void queue_free_task(void *data, Handle h) {
  //
}

State poll_task(void *data, Handle pollee) {
  TaskPool *p = global_pool();
  return p->tasks[pollee.idx - 1].state;
}

void busy_wait_ready(void *data, Handle h) {
  while (poll_state(h) != READY) {
    async_skip();
  }
}

Handle current_task(void *data) {
  Queue *q = data;
  Handle h = {0};
  queue_peek_front(q, &h);
  return h;
}

Handle next_task(void *data) {
  Queue *q = data;
  Handle h = {0};
  queue_pop_front(q, &h);
  queue_push_back(q, h);
  queue_peek_front(q, &h);
  return h;
}

void cleanup(void *data) {
  Queue *q = data;
  if (q->cap)
    free(q->elems);
  *q = (Queue){0};
}

static SchedulerVTable vtable = {
    .register_task = register_task,
    .free_task = queue_free_task,
    .finish_task = finish_task,
    .poll_task = poll_task,
    .wait_ready = busy_wait_ready,
    .current_task = current_task,
    .next_task = next_task,
    .cleanup = cleanup,
};
static Queue queue = {0};

void use_queue_scheduler() {
  Scheduler *s = global_scheduler();
  s->vtable = &vtable;
  s->data = &queue;
}
