#include "scheduler.h"
#include "storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static FunctionStore functions = {0};
static Queue event_queue = {0};

Handle current(void *data) { return peek_queue(&event_queue); }
void next_coroutine(void *data) {
  Handle h = dequeue(&event_queue);
  enqueue(&event_queue, h);
}
FunctionObject *get_coroutine(void *data, Handle h) {
  return &functions.elems[h.idx - 1];
}
Handle add_coroutine(void *data, FunctionObject elem) {
  Handle h = new_handle(&functions, elem);
  enqueue(&event_queue, h);
  return h;
}
void free_coroutine(void *data, Handle h) { free_handle(&functions, h); }

void cleanup(void *data) { deinit_store(&functions); }

Handle terminate_coroutine(void *data) { return dequeue(&event_queue); }

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
  functions = (FunctionStore){0};
  event_queue = (Queue){0};
  s->data = NULL;
  s->vtable = &vtable;
}
