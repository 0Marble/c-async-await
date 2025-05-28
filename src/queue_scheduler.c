#include "scheduler.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define FUNCTIONS_COUNT 65536
#define QUEUE_SIZE 65536

typedef struct {
  FunctionObject elems[FUNCTIONS_COUNT];
  int count;
  int next_free;
} Array;

static Array functions = {0};

typedef struct {
  Handle buf[QUEUE_SIZE];
  int start, end;
} Queue;

static Queue event_queue = {0};

Handle dequeue() {
  assert(event_queue.start != event_queue.end && "Queue underflow!");
  Handle h = event_queue.buf[event_queue.start];
  assert(h.idx != 0);
  event_queue.buf[event_queue.start].idx = 0;
  event_queue.start = (event_queue.start + 1) % QUEUE_SIZE;
  return h;
}

void enqueue(Handle h) {
  assert(h.idx != 0);
  assert(event_queue.buf[event_queue.end].idx == 0);
  event_queue.buf[event_queue.end] = h;
  event_queue.end = (event_queue.end + 1) % QUEUE_SIZE;
  assert(event_queue.end != event_queue.start && "Queue overflow!");
}

Handle peek() { return event_queue.buf[event_queue.start]; }

Handle push(FunctionObject f) {
  if (functions.next_free == 0) {
    DBG("Creating new function object", 0);
    f.this_fn = (Handle){.idx = functions.count + 1};
    assert(functions.count < FUNCTIONS_COUNT);
    functions.elems[functions.count] = f;
    functions.count++;
    return f.this_fn;
  } else {
    DBG("Reusing function object %d", functions.next_free);
    f.this_fn = (Handle){.idx = functions.next_free};
    FunctionObject *old = &functions.elems[functions.next_free - 1];
    assert(old->state == DEAD);
    int next_free = old->this_fn.idx;
    *old = f;
    functions.next_free = next_free;
    return f.this_fn;
  }
}

void free_handle(void *data, Handle h) {
  assert(h.idx != 0);
  assert(h.idx != 1 && "Dont free async_main");

  FunctionObject *f = &functions.elems[h.idx - 1];
  assert(f->state != DEAD && "Double free!");

  int next_free = f->this_fn.idx;
  f->state = DEAD;
  f->this_fn.idx = functions.next_free;
  f->stack_top = f->stack + STACK_SIZE;
  functions.next_free = next_free;
}

Handle current(void *data) { return peek(); }
void next_coroutine(void *data) {
  Handle h = dequeue();
  enqueue(h);
}
FunctionObject *get_coroutine(void *data, Handle h) {
  return &functions.elems[h.idx - 1];
}
Handle add_coroutine(void *data, FunctionObject elem) {
  Handle h = push(elem);
  enqueue(h);
  return h;
}
void free_coroutine(void *data, Handle h) {
  assert(h.idx != 0);
  assert(h.idx != 1 && "Dont free async_main");

  FunctionObject *f = &functions.elems[h.idx - 1];
  assert(f->state != DEAD && "Double free!");

  int next_free = f->this_fn.idx;
  f->state = DEAD;
  f->this_fn.idx = functions.next_free;
  f->stack_top = f->stack + STACK_SIZE;
  functions.next_free = next_free;
}

void cleanup(void *data) {
  for (int i = 0; i < functions.count; i++) {
    FunctionObject *f = &functions.elems[i];
    if (f->stack && munmap(f->stack, STACK_SIZE) == -1) {
      perror("munmap: ");
    }
  }
}

Handle terminate_coroutine(void *data) { return dequeue(); }

static SchedulerVTable vtable = {
    .current = current,
    .next_coroutine = next_coroutine,
    .get_coroutine = get_coroutine,
    .add_coroutine = add_coroutine,
    .free_coroutine = free_coroutine,
    .cleanup = cleanup,
    .terminate_coroutine = terminate_coroutine,
};

void init_queue_scheduler(Scheduler *s) {
  s->data = NULL;
  s->vtable = &vtable;
}
