#ifndef __ASYNC_H__
#define __ASYNC_H__

typedef struct {
  int idx;
} Handle;

typedef void AsyncFunction(Handle, void *);

void *run_async_main(AsyncFunction *main_fn, void *arg);

Handle async_call(AsyncFunction *f, void *arg);
void *await(Handle other_fn);
void async_return(void *data);

#endif
