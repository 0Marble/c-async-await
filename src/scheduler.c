#include "scheduler.h"
#include "async.h"
#include "dbg.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

Handle start_new_task(AsyncFunction *fn, void *data) {
  TaskPool *pool = global_pool();
  Handle h = {.idx = 0};
  if (pool->free_task.idx == 0) {

    if (pool->len == pool->cap) {
      pool->cap = pool->cap * 2 + 10;
      pool->tasks = realloc(pool->tasks, pool->cap * sizeof(Task));
      assert(pool->tasks);
    }

    void *stack_base =
        mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_STACK | MAP_GROWSDOWN | MAP_ANONYMOUS, -1, 0);
    if (stack_base == MAP_FAILED) {
      perror("mmap: ");
      exit(1);
    }

    Task *new_task = &pool->tasks[pool->len];
    pool->len++;
    h.idx = pool->len;
    new_task->stack_base = stack_base;
    new_task->stack_ptr = stack_base + STACK_SIZE;
    new_task->fn = fn;
    new_task->data = data;
    new_task->state = INIT;
    new_task->orphaned = false;
    new_task->handle = h;
  } else {
    h = pool->free_task;
    assert(h.idx > 0);
    assert(h.idx <= pool->len);
    Task *t = &pool->tasks[h.idx - 1];
    assert(t->state == FREE);
    assert(t->stack_base);
    pool->free_task = t->handle;

    t->state = INIT;
    t->handle = h;
    t->fn = fn;
    t->data = data;
    t->stack_ptr = t->stack_base + STACK_SIZE;
  }

  Scheduler *scheduler = global_scheduler();
  scheduler->vtable->register_task(scheduler->data, h);
  return h;
}

Task *get_task(Handle h) {
  assert(h.idx > 0);
  assert(h.idx <= global_pool()->len);
  return &global_pool()->tasks[h.idx - 1];
}

State poll_state(Handle h) {
  Scheduler *s = global_scheduler();
  Handle cur = current_task_handle();
  s->vtable->poll_task(s->data, h);
  Task *t = get_task(h);
  return t->state;
}

void wait_ready(Handle h) {
  Scheduler *s = global_scheduler();
  s->vtable->wait_ready(s->data, h);
}

void free_task(Handle h) {
  Scheduler *s = global_scheduler();
  TaskPool *p = global_pool();
  assert(h.idx);
  s->vtable->free_task(s->data, h);

  Task *t = get_task(h);
  t->state = FREE;
  t->handle = p->free_task;
  p->free_task = h;
}

void finish_current_task(Handle *finished_task, Handle *next_task) {
  Scheduler *s = global_scheduler();
  s->vtable->finish_task(s->data, finished_task, next_task);
}

Handle current_task_handle() {
  Scheduler *s = global_scheduler();
  return s->vtable->current_task(s->data);
}

Handle next_task_handle() {
  Scheduler *s = global_scheduler();
  return s->vtable->next_task(s->data);
}

void async_deinit() {
  Scheduler *s = global_scheduler();
  TaskPool *p = global_pool();
  s->vtable->cleanup(s->data);

  for (int i = 0; i < p->len; i++) {
    Task *t = &p->tasks[i];
    // if (munmap(t->stack_base, STACK_SIZE) == -1) {
    //   perror("munmap");
    // }
  }
  free(p->tasks);
  *p = (TaskPool){0};
}

static Scheduler scheduler = {0};
static TaskPool pool = {0};

Scheduler *global_scheduler() { return &scheduler; }

TaskPool *global_pool() { return &pool; }

void async_init() {
  // use_queue_scheduler();
  use_graph_scheduler();
}
