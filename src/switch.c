#include "async.h"
#include "dbg.h"
#include "scheduler.h"
#include <assert.h>
#include <stddef.h>
#include <sys/mman.h>

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

void async_switch(Handle from, Handle to) {
  assert(to.idx != 0);
  DBG("switch %d -> %d", from.idx, to.idx);
  assert(from.idx != to.idx);

  Task *f1 = (from.idx == 0 ? NULL : get_task(from));
  Task *f2 = get_task(to);

  long f2_first_call = f2->state == INIT;
  assert(f2->state != FREE);
  assert(f2->state != READY);

  if (f2_first_call) {
    DBG("%d starting for the first time", to.idx);
    f2->state = RUNNING;
  }
  assert(f2->stack_ptr != NULL);

  if (f1) {
    async_switch_asm(&f1->stack_ptr, f2->stack_ptr, f2_first_call, f2->fn,
                     f2->data);
  } else {
    async_switch_asm(NULL, f2->stack_ptr, f2_first_call, f2->fn, f2->data);
  }

  return;
}
