#include "async.h"
#include "scheduler.h"
#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static Scheduler scheduler = {0};

void async_free(Handle h) {
  scheduler.vtable->free_coroutine(scheduler.data, h);
}

void async_orphan(Handle h) {
  FunctionObject *f = scheduler.vtable->get_coroutine(scheduler.data, h);
  f->orphaned = true;
}

Handle async_call(AsyncFunction *f, void *arg) {
  FunctionObject elem = {0};
  elem.fn = f;
  elem.data = arg;
  elem.state = INIT;
  elem.stack = NULL;
  elem.stack_top = NULL;
  elem.orphaned = false;

  Handle h = scheduler.vtable->add_coroutine(scheduler.data, elem);

  return h;
}

void async_switch_asm(void **f1_stack_ptr, // rdi
                      void *f2_stack_ptr,  // rsi
                      long f2_first_start, // rdx
                      void *f2_fn,         // rcx
                      void *f2_arg         // r8
) {
  asm volatile("\n"
               "pushq %rbx\n"
               "pushq %r12\n"
               "pushq %r13\n"
               "pushq %r14\n"
               "pushq %r15\n"
               "pushq %rbp\n"
               "cmpq $0x0, %rdi\n"
               "je 2f\n"
               "movq %rsp, (%rdi)\n"
               "2:\n"
               "movq %rsi, %rsp\n"
               "cmpq $0x0, %rdx\n"
               "je 1f\n"
               "movq %r8, %rdi\n"
               "call *%rcx\n"
               "int $3\n"
               "1:\n"
               "popq %rbp\n"
               "popq %r15\n"
               "popq %r14\n"
               "popq %r13\n"
               "popq %r12\n"
               "popq %rbx\n"
               "\n");
}

void async_switch(Handle from) {
  Handle to = scheduler.vtable->current(scheduler.data);
  assert(to.idx != 0);
  DBG("%d to %d", from.idx, to.idx);

  FunctionObject *f1 = NULL;
  if (from.idx != 0) {
    f1 = scheduler.vtable->get_coroutine(scheduler.data, from);
  }
  FunctionObject *f2 = scheduler.vtable->get_coroutine(scheduler.data, to);
  long f2_first_call = f2->state == INIT;
  assert(f2->state != DEAD);
  assert(f2->state != READY);

  if (f2_first_call) {
    DBG("%d starting for the first time", f2->this_fn.idx);
    if (f2->stack == NULL) {
      void *stack =
          mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_STACK | MAP_GROWSDOWN | MAP_ANONYMOUS, -1, 0);
      if (stack == MAP_FAILED) {
        perror("mmap: ");
        exit(1);
      }
      DBG("%d allocated new stack: %p -- %p", f2->this_fn.idx, stack,
          stack + STACK_SIZE);
      f2->stack = stack;
      f2->stack_top = stack + STACK_SIZE;
    } else {
      DBG("%d reuses stack at %p", f2->stack);
    }
    f2->state = RUNNING;
  }
  assert(f2->stack_top != NULL);
  if (f1) {
    async_switch_asm(&f1->stack_top, f2->stack_top, f2_first_call, f2->fn,
                     f2->data);
  } else {
    async_switch_asm(NULL, f2->stack_top, f2_first_call, f2->fn, f2->data);
  }
  return;
}

void *run_async_main(AsyncFunction *main_fn, void *arg) {
  init_queue_scheduler(&scheduler);

  Handle h = async_call(main_fn, arg);
  FunctionObject *f0 = scheduler.vtable->get_coroutine(scheduler.data, h);
  asm volatile("\n"
               "movq %%rsp, %0\n"
               : "=r"(f0->stack_top)
               :
               : "memory");
  f0->state = RUNNING;
  main_fn(arg);
  assert(f0->state == READY);

  scheduler.vtable->cleanup(scheduler.data);

  return f0->data;
}

void *await(Handle other_fn) {
  while (true) {
    Handle this_fn = scheduler.vtable->current(scheduler.data);
    assert(this_fn.idx != 0);
    FunctionObject *f0 =
        scheduler.vtable->get_coroutine(scheduler.data, this_fn);
    FunctionObject *f1 =
        scheduler.vtable->get_coroutine(scheduler.data, other_fn);

    if (f0->state == SLEEPING) {
      f0->state = RUNNING;
      return NULL;
    }

    DBG("%d waits for %d", this_fn.idx, other_fn.idx);

    if (other_fn.idx == 0) {
      f0->state = SLEEPING;
    } else if (f1->state == READY) {
      return f1->data;
    }

    scheduler.vtable->next_coroutine(scheduler.data);
    async_switch(this_fn);
    DBG("switched to %d", this_fn.idx);
  }
}

void async_return(void *data) {
  Handle this_fn = scheduler.vtable->terminate_coroutine(scheduler.data);
  FunctionObject *f = scheduler.vtable->get_coroutine(scheduler.data, this_fn);
  DBG("%p from %d", data, this_fn.idx);

  assert(this_fn.idx != 0);
  assert(f->state == RUNNING);
  f->state = READY;
  f->data = data;

  // async_main
  if (this_fn.idx == 1) {
    return;
  }
  scheduler.vtable->next_coroutine(scheduler.data);
  if (f->orphaned) {
    async_free(this_fn);
    async_switch((Handle){.idx = 0});
  } else {
    async_switch(this_fn);
  }
  assert(false && "Unreachable");
}

void async_skip() {
  Handle zero_handle = {.idx = 0};
  (void)await(zero_handle);
}

void *await_any(Handle *handles, int len, int *result_idx) {
  while (true) {
    for (int i = 0; i < len; i++) {
      Handle h = handles[i];
      assert(h.idx != 0);
      FunctionObject *f = scheduler.vtable->get_coroutine(scheduler.data, h);
      if (f->state == READY) {
        if (result_idx)
          *result_idx = i;
        return f->data;
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

      FunctionObject *f = scheduler.vtable->get_coroutine(scheduler.data, h);
      if (f->state == READY) {
        DBG("%d ready", h.idx);
        if (results)
          results[i] = f->data;
        succ_cnt++;
      }
    }
    DBG("succ_cnt=%d", succ_cnt);

    if (succ_cnt == len)
      break;
    async_skip();
  }
}

Handle current_handle() { return scheduler.vtable->current(scheduler.data); }

int pack(void *buf, int size, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  assert(size >= 2 * sizeof(int));
  int ptr = 2 * sizeof(int);

  while (true) {
    char c = *fmt;
    if (c == 0)
      break;
    fmt++;

    switch (c) {
    case 'i': {
      int32_t n = va_arg(ap, int32_t);
      assert(size >= ptr);
      memcpy(buf + ptr, &n, sizeof(int32_t));
      ptr += sizeof(int32_t);
    } break;
    case 'u': {
      uint32_t n = va_arg(ap, uint32_t);
      assert(size >= ptr);
      memcpy(buf + ptr, &n, sizeof(uint32_t));
      ptr += sizeof(uint32_t);
    } break;
    case 'p': {
      long n = (long)va_arg(ap, void *);
      assert(size >= ptr);
      memcpy(buf + ptr, &n, sizeof(long));
      ptr += sizeof(void *);
    } break;

    default: {
      DBG("Invalid type: %c", c);
      return -1;
    } break;
    }
  }
  memcpy(buf, &ptr, sizeof(ptr));
  memcpy(buf + sizeof(int), &size, sizeof(size));
  return ptr;
}

int unpack(void *buf, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = *(int *)buf;
  int size = *(int *)(buf + sizeof(int));
  int ptr = 2 * sizeof(int);

  while (true) {
    char c = *fmt;
    if (c == 0)
      break;
    fmt++;
    assert(len > ptr);

    switch (c) {
    case 'i': {
      int32_t *n = va_arg(ap, int32_t *);
      memcpy(n, buf + ptr, sizeof(int32_t));
      ptr += sizeof(int32_t);
    } break;
    case 'u': {
      uint32_t *n = va_arg(ap, uint32_t *);
      memcpy(n, buf + ptr, sizeof(uint32_t));
      ptr += sizeof(uint32_t);
    } break;
    case 'p': {
      void **n = va_arg(ap, void **);
      memcpy(n, buf + ptr, sizeof(void *));
      ptr += sizeof(void *);
    } break;

    default: {
      DBG("Invalid type: %c", c);
      return -1;
    } break;
    }
  }
  assert(ptr == len);
  return size;
}
