#include "async.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

void fib(Handle h, void *data) {
  long n = (long)data;
  printf("Fib[%d](%ld): starting\n", h.idx, n);
  if (n < 2)
    async_return(data);

  long n1 = (long)await(async_call(fib, (void *)(n - 1)));
  printf("Fib[%d](%ld): got n1\n", h.idx, n);
  long n2 = (long)await(async_call(fib, (void *)(n - 2)));
  printf("Fib[%d](%ld): got n2\n", h.idx, n);

  async_return((void *)(n1 + n2));
}

void async_main(Handle h, void *data) {
  printf("async_main(%d): started\n", h.idx);
  // 0 1 1 2 3 5 8 13 21 34 55
  long x = (long)await(async_call(fib, (void *)10));
  printf("async_main(%d): got result from fib: %ld\n", h.idx, x);

  async_return((void *)0);
}

int main(int argc, char **argv) {
  long res = (long)run_async_main(async_main, NULL);
  return res;
}
