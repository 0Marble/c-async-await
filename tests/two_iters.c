
/*
$$ gcc -o  ./build/tests/two_iters ./tests/two_iters.c -I src -L build -lasync
!! ./build/tests/two_iters

%% 0 5 -5 0
## 0 -5 1 -4 2 -3 3 -2 4 -1 \
-------
 */

#include "../src/async.h"
#include <stddef.h>
#include <stdio.h>

void iterate(void *args) {
  int a = ((int *)args)[0], b = ((int *)args)[1];
  for (int i = a; i < b; i++) {
    printf("%d ", i);
    async_skip();
  }
  async_return(NULL);
}

void async_main(void *args) {
  int a[2] = {0};
  int b[2] = {0};
  scanf("%d %d %d %d", &a[0], &a[1], &b[0], &b[1]);

  Handle handles[2];
  handles[0] = async_call(iterate, a);
  handles[1] = async_call(iterate, b);
  await_all(handles, 2, NULL);
  putc('\\', stdout);

  async_return(NULL);
}

int main(int argc, char *argv[]) {
  run_async_main(async_main, NULL);
  return 0;
}
