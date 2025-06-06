#include "async.h"
#include "io.h"
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

void async_sleep(void *data) {
  printf("%s(%ld): started\n", __func__, (long)data);

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
        .tv_nsec = 100000000,
    };
    if (nanosleep(&sleep_delay, NULL) < 0) {
      perror("nanosleep:");
    }
  }

  async_return(data);
}

void async_add2(void *data) {
  int a = 0, b = 0;
  int size = unpack(data, "ii", &a, &b);
  pack(data, size, "i", a + b);
  async_return(data);
}

void async_sum(void *data) {
  int n = 0;
  int size = (int)(long)data;
  printf("%s(%d): started\n", __func__, n);

  long sum = 0;
  char buf[256] = {0};
  for (int i = 0; i < n; i++) {
    pack(buf, sizeof(buf), "ii", sum, i);
    Handle h = async_call(async_add2, buf);
    unpack(await(h), "i", &sum);
    async_free(h);
    printf("%s: %d/%d summed\n", __func__, i, n);
  }
  async_return((void *)(long)sum);
}

void async_main(void *data) {
  printf("%s(): started\n", __func__);

  Handle handles[2] = {0};
  handles[0] = async_call(async_sleep, (void *)1);
  handles[1] = async_call(async_sum, (void *)10);
  void *results[2] = {0};

  await_all(handles, 2, results);

  int sum = (int)(long)results[1];
  printf("%s: async_sleep=%ld, async_sum=%d\n", __func__, (long)results[0],
         sum);

  async_return((void *)0);
}

int main(int argc, char **argv) { run_async_main(async_main, NULL); }
