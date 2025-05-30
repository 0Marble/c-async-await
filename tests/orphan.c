
/*
$$ gcc -fPIC -ggdb -fsanitize=address
$$ -o ./build/tests/orphan ./tests/orphan.c -I src -L build -lasync
!! ./build/tests/orphan

%%
## Client: 0
## Client: 1
## Client: 2
## Client: 3
## Client: 4
## Client: 5
## Client: 6
## Client: 7
## Client: 8
## Client: 9
## Max handle: 2
##
-------
 */

#include "../src/async.h"
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

void client(void *args) {
  printf("Client: %d\n", (int)(long)args);
  async_return(NULL);
}

void async_main(void *args) {
  int max = 0;
  for (int i = 0; i < 10; i++) {
    Handle h = async_call(client, (void *)(long)i);
    async_orphan(h);
    if (max < h.idx) {
      max = h.idx;
    }
    async_skip();
  }
  printf("Max handle: %d\n", max);
  async_return(NULL);
}

int main(int argc, char *argv[]) {
  run_async_main(async_main, NULL);
  return 0;
}
