#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "async.h"

typedef Handle CurrentCoroutine(void *scheduler);
typedef void NextCoroutine(void *scheduler);
typedef FunctionObject *GetCoroutine(void *scheduler, Handle h);
typedef Handle AddCoroutine(void *scheduler, FunctionObject crt);
typedef void FreeCoroutine(void *scheduler, Handle h);
typedef Handle TerminateCoroutine(void *scheduler);
typedef void Cleanup(void *scheduler);

typedef struct {
  // returns the currently active coroutine
  CurrentCoroutine *current;
  // Leaves the current coroutine for later, sets another one to active.
  NextCoroutine *next_coroutine;
  // get a FunctionObject* by handle
  GetCoroutine *get_coroutine;
  // add a new FunctionObject
  AddCoroutine *add_coroutine;
  // free up a handle
  FreeCoroutine *free_coroutine;
  // deinit the whole thing
  Cleanup *cleanup;
  // stops the current coroutine, but not frees
  TerminateCoroutine *terminate_coroutine;
} SchedulerVTable;

typedef struct {
  void *data;
  SchedulerVTable *vtable;
} Scheduler;

void init_queue_scheduler(Scheduler *s);

#endif
