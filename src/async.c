#include "async.h"
#include "dbg.h"
#include "scheduler.h"
#include "switch.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

void async_free(Handle h) { free_task(h); }

void async_orphan(Handle h) {
  Task *t = get_task(h);
  t->orphaned = true;
}

Handle async_call(AsyncFunction *f, void *arg) {
  Handle h = start_new_task(f, arg);
  DBG("new coroutine: %d", h.idx);
  return h;
}

void run_async_main(AsyncFunction *main_fn, void *arg) {
  async_init();
  Handle h = async_call(main_fn, arg);
  assert(h.idx == 1);
  async_switch((Handle){.idx = 0}, h);
  assert(false);
}

void *await(Handle h) {
  while (poll_state(h) != READY) {
    async_skip();
  }
  Task *t = get_task(h);
  return t->data;
}

void async_return(void *data) {
  Handle finished_task = {0};
  Handle next_task = {0};
  finish_current_task(&finished_task, &next_task);
  DBG("%d finished with %p", finished_task.idx, data);
  if (finished_task.idx == 1) {
    exit((int)(long)data);
  }
  Task *t = get_task(finished_task);
  t->data = data;
  t->state = READY;
  if (t->orphaned) {
    async_free(finished_task);
  }
  async_switch((Handle){0}, next_task);
  assert(false);
}

void async_skip() {
  Handle current = current_task_handle();
  Handle next = next_task_handle();
  Task *t1 = get_task(current);

  async_switch(current, next);
}

void *await_any(Handle *handles, int len, int *result_idx) {
  while (true) {
    for (int i = 0; i < len; i++) {
      Handle h = handles[i];
      assert(h.idx != 0);
      if (poll_state(h) == READY) {
        Task *t = get_task(h);
        if (result_idx)
          *result_idx = i;
        return t->data;
      }
    }
    async_skip();
  }
}

void await_all(Handle *handles, int len, void **results) {
  while (true) {
    int succ_cnt = 0;
    for (int i = 0; i < len; i++) {
      Handle h = handles[i];
      assert(h.idx != 0);
      if (poll_state(h) == READY) {
        Task *t = get_task(h);
        if (results)
          results[i] = t->data;
        succ_cnt++;
      }
    }
    DBG("succ_cnt=%d", succ_cnt);

    if (succ_cnt == len)
      break;
    async_skip();
  }
}
