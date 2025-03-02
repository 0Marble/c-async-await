#include "async.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void async_sleep(void *data) {
  printf("%s(%ld): started\n", __func__, (long)data);
  assert(current_handle().idx == 2);

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

void async_add2(void *data) {
  long *ab = data;
  printf("%s(%ld,%ld): started\n", __func__, ab[0], ab[1]);

  async_return((void *)(ab[0] + ab[1]));
}

void async_sum(void *data) {
  printf("%s(%ld): started\n", __func__, (long)data);
  assert(current_handle().idx == 3);

  long sum = 0;
  for (long i = 0; i < (long)data; i++) {
    long ab[2] = {sum, i};
    sum = (long)await(async_call(async_add2, ab));
    printf("%s: %ld/%ld summed\n", __func__, i, (long)data);
  }
  async_return((void *)sum);
}

void async_main(void *data) {
  printf("%s(): started\n", __func__);
  assert(current_handle().idx == 1);

  Handle handles[2] = {0};
  handles[0] = async_call(async_sleep, (void *)1);
  handles[1] = async_call(async_sum, (void *)100);
  void *results[2] = {0};

  await_all(handles, 2, results);

  printf("%s: async_sleep=%ld, do_stuff=%ld\n", __func__, (long)results[0],
         (long)results[1]);
  assert((long)results[0] == 69);
  assert((long)results[1] == 4950);

  async_return((void *)0);
}

int main(int argc, char **argv) {
  long res = (long)run_async_main(async_main, NULL);
  return res;
}
