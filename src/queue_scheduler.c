#include "scheduler.h"
#include <assert.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef struct {
  Handle *elems;
  int len;
  int cap;
} Stack;

void stack_push(Stack *s, Handle h) {
  assert(h.idx != 0);
  if (s->len == s->cap) {
    s->cap = s->cap * 2 + 10;
    s->elems = realloc(s->elems, s->cap * sizeof(Handle));
    assert(s->elems);
  }
  s->elems[s->len++] = h;
}

Handle stack_pop(Stack *s) {
  if (s->len == 0)
    return (Handle){0};
  s->len--;
  Handle h = s->elems[s->len];
  return h;
}

Handle stack_peek(Stack *s) {
  if (s->len == 0)
    return (Handle){0};
  return s->elems[s->len - 1];
}

typedef struct {
  Stack push;
  Stack pop;
} Queue;

void queue_push(Queue *q, Handle h) {
  assert(h.idx);
  stack_push(&q->push, h);
}

Handle queue_pop(Queue *q) {
  if (q->pop.len == 0) {
    while (true) {
      Handle h = stack_pop(&q->push);
      if (h.idx == 0)
        break;
      stack_push(&q->pop, h);
    }
  }
  return stack_pop(&q->pop);
}

Handle queue_peek(Queue *q) {
  if (q->pop.len == 0) {
    if (q->push.len == 0)
      return (Handle){0};
    else
      return q->push.elems[0];
  } else {
    return stack_peek(&q->pop);
  }
}

void register_task(void *data, Handle h) { queue_push(data, h); }

void finish_task(void *data, Handle *cur, Handle *next) {
  *cur = queue_pop(data);
  *next = queue_peek(data);
}

void queue_free_task(void *data, Handle h) {
  //
}

State poll_task(void *data, Handle pollee) {
  TaskPool *p = global_pool();
  return p->tasks[pollee.idx - 1].state;
}

Handle current_task(void *data) { return queue_peek(data); }

Handle next_task(void *data) {
  Handle cur = queue_pop(data);
  queue_push(data, cur);
  return queue_peek(data);
}

void cleanup(void *data) {
  Queue *q = data;
  if (q->pop.cap)
    free(q->pop.elems);
  if (q->push.cap)
    free(q->push.elems);
  *q = (Queue){0};
}

static SchedulerVTable vtable = {
    .register_task = register_task,
    .free_task = queue_free_task,
    .finish_task = finish_task,
    .poll_task = poll_task,
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
