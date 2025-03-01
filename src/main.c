#include "async.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void async_sleep(Handle h, void *data) {
  long start = time(NULL);
  long delay = (long)data;

  while (true) {
    long now = time(NULL);
    printf("%s: %ld seconds left\n", __func__, delay - (now - start));
    if (start + delay <= now)
      break;
    async_skip();
    struct timespec sleep_delay = {
        .tv_sec = 0,
        .tv_nsec = 10000000, // 0.01s
    };
    if (nanosleep(&sleep_delay, NULL) < 0) {
      perror("nanosleep:");
    }
  }

  async_return((void *)69);
}

void async_ident(Handle h, void *data) { async_return(data); }

void do_stuff(Handle h, void *data) {
  long sum = 0;
  for (long i = 0; i < (long)data; i++) {
    long x = (long)await(async_call(async_ident, (void *)i));
    printf("%s: %ld/%ld summed\n", __func__, i, (long)data);
    sum += x;
  }
  async_return((void *)sum);
}

void async_main(Handle h, void *data) {
  printf("async_main(%d): started\n", h.idx);

  Handle handles[2] = {0};
  handles[0] = async_call(async_sleep, (void *)1);
  handles[1] = async_call(do_stuff, (void *)100);
  void *results[2] = {0};

  await_all(handles, 2, results);

  printf("%s: async_sleep=%ld, do_stuff=%ld\n", __func__, (long)results[0],
         (long)results[1]);

  async_return((void *)0);
}

int main(int argc, char **argv) {
  long res = (long)run_async_main(async_main, NULL);
  return res;
}
