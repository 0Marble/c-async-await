#include "async.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void foo(Handle h, void *data) {
  long x = (long)data;
  printf("Hello from Foo(%d)! Input: %ld\n", h.idx, x);
  async_return((void *)-x);
}

void async_main(Handle h, void *data) {
  printf("async_main(%d): started\n", h.idx);
  long x = (long)await(async_call(foo, (void *)1000));
  printf("async_main(%d): got result from foo: %ld\n", h.idx, x);

  async_return((void *)0);
}

int main(int argc, char **argv) {
  long res = (long)run_async_main(async_main, NULL);
  return res;
}
