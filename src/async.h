#ifndef __ASYNC_H__
#define __ASYNC_H__

#ifdef DEBUG
#define DBG(fmt, args...)                                                      \
  do {                                                                         \
    fprintf(stderr, "[%s:%d] %s: " fmt "\n", __FILE_NAME__, __LINE__,          \
            __func__, args);                                                   \
  } while (0)
#else
#define DBG(fmt, args...)
#endif

typedef struct {
  int idx;
} Handle;

typedef void AsyncFunction(void *);

enum { INIT, RUNNING, SLEEPING, READY, DEAD };
#ifndef STACK_SIZE
#define STACK_SIZE 4 * 4096
#endif

typedef struct {
  void *stack;
  void *stack_top;
  AsyncFunction *fn;
  int state;
  Handle this_fn;
  void *data;
} FunctionObject;

void *run_async_main(AsyncFunction *main_fn, void *arg);

Handle async_call(AsyncFunction *f, void *arg);
void *await(Handle other_fn);
void async_return(void *data);
void async_free(Handle h);

void *await_any(Handle *handles, int len, int *res_idx);
void await_all(Handle *handles, int len, void **results);

void async_skip();
Handle current_handle();

int pack(void *buf, int size, const char *fmt, ...);
int unpack(void *buf, const char *fmt, ...);

#endif
