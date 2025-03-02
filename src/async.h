#ifndef __ASYNC_H__
#define __ASYNC_H__

typedef struct {
  int idx;
} Handle;

typedef void AsyncFunction(void *);

void *run_async_main(AsyncFunction *main_fn, void *arg);

Handle async_call(AsyncFunction *f, void *arg);
void *await(Handle other_fn);
void async_return(void *data);
void async_free(Handle h);

void *await_any(Handle *handles, int len, int *res_idx);
void await_all(Handle *handles, int len, void **results);

void async_skip();
Handle current_handle();

#endif
