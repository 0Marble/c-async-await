#include "async.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

const int STACK_SIZE = 4096;
#define FUNCTIONS_COUNT 1024
#define QUEUE_SIZE 100

#ifdef DEBUG
#define DBG(fmt, args...)                                                      \
  do {                                                                         \
    printf("[%s:%d] %s: " fmt, __FILE_NAME__, __LINE__, __func__, args);       \
  } while (0)
#else
#define DBG(fmt, args...)
#endif

enum { INIT, RUNNING, SLEEPING, READY };

typedef struct {
  void *stack;
  void *stack_top;
  AsyncFunction *fn;
  int state;
  Handle this_fn;
  void *data;
} FunctionInfo;

typedef struct {
  FunctionInfo elems[FUNCTIONS_COUNT];
  int count;
} Array;

Array functions = {0};

typedef struct {
  Handle buf[QUEUE_SIZE];
  int start, end;
} Queue;

Queue event_queue = {0};

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

void cleanup() {
  for (int i = 0; i < functions.count; i++) {
    FunctionInfo *f = &functions.elems[i];
    if (f->stack == NULL) {
      continue;
    }

    if (munmap(f->stack, STACK_SIZE) == -1) {
      perror("munmap: ");
    }
  }
}

Handle async_call(AsyncFunction *f, void *arg) {
  Handle h = {.idx = functions.count + 1};

  FunctionInfo elem = {
      .fn = f,
      .data = arg,
      .state = INIT,
      .this_fn = h,
      .stack = NULL,
      .stack_top = NULL,
  };
  assert(functions.count < FUNCTIONS_COUNT);
  functions.elems[functions.count] = elem;
  functions.count++;
  enqueue(h);

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
               "movq %rsp, (%rdi)\n"
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
  assert(from.idx != 0);
  Handle to = peek();
  assert(to.idx != 0);
  DBG("%d to %d\n", from.idx, to.idx);

  FunctionInfo *f1 = &functions.elems[from.idx - 1];
  FunctionInfo *f2 = &functions.elems[to.idx - 1];
  long f2_first_call = f2->state == INIT;

  if (f2_first_call) {
    assert(f2->stack_top == NULL);

    DBG("%d starting for the first time\n", f2->this_fn.idx);
    void *stack =
        mmap(NULL, STACK_SIZE, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_STACK | MAP_GROWSDOWN | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) {
      perror("mmap: ");
      exit(1);
    }
    f2->stack = stack;
    long *f2_stack = stack + STACK_SIZE;
    f2->stack_top = f2_stack;
    f2->state = RUNNING;
  }
  assert(f2->stack_top != NULL);
  async_switch_asm(&f1->stack_top, f2->stack_top, f2_first_call, f2->fn,
                   f2->data);
  return;
}

void *run_async_main(AsyncFunction *main_fn, void *arg) {
  Handle h = async_call(main_fn, arg);
  FunctionInfo *f0 = &functions.elems[h.idx - 1];
  asm volatile("\n"
               "movq %%rsp, %0\n"
               : "=r"(f0->stack_top)
               :
               : "memory");
  f0->state = RUNNING;
  main_fn(arg);
  assert(f0->state == READY);

  cleanup();

  return f0->data;
}

void *await(Handle other_fn) {
  while (true) {
    Handle this_fn = peek();
    assert(this_fn.idx != 0);
    FunctionInfo *f0 = &functions.elems[this_fn.idx - 1];

    if (f0->state == SLEEPING) {
      f0->state = RUNNING;
      return NULL;
    }

    DBG("%d waits for %d\n", this_fn.idx, other_fn.idx);

    if (other_fn.idx == 0) {
      f0->state = SLEEPING;
    } else if (functions.elems[other_fn.idx - 1].state == READY) {
      return functions.elems[other_fn.idx - 1].data;
    }

    assert(dequeue().idx == this_fn.idx);
    enqueue(this_fn);
    async_switch(this_fn);
    DBG("switched to %d\n", this_fn.idx);
  }
}

void async_return(void *data) {
  Handle this_fn = dequeue();
  DBG("%p from %d\n", data, this_fn.idx);

  assert(this_fn.idx != 0);
  assert(functions.elems[this_fn.idx - 1].state == RUNNING);
  functions.elems[this_fn.idx - 1].state = READY;
  functions.elems[this_fn.idx - 1].data = data;

  // async_main
  if (this_fn.idx == 1) {
    return;
  }
  async_switch(this_fn);
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
      FunctionInfo *f = &functions.elems[h.idx - 1];
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
      FunctionInfo *f = &functions.elems[h.idx - 1];
      if (f->state == READY) {
        results[i] = f->data;
        succ_cnt++;
      }
    }

    if (succ_cnt == len)
      break;
    async_skip();
  }
}

Handle current_handle() { return peek(); }
