#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

#include "async.h"
#include "stdbool.h"

typedef enum {
  INIT,    // coroutine was just created
  READY,   // coroutine has finished
  RUNNING, // coroutine is in progress
  FREE,    // freed coroutine, should be unreachable
} State;

typedef struct {
  void *stack_base;
  void *stack_ptr;
  AsyncFunction *fn;
  void *data;
  State state;
  bool orphaned;
  Handle handle; // if state is FREE, this points to the next free task
} Task;

typedef struct {
  Task *tasks;
  int len;
  int cap;
  Handle free_task; // handle of the first free task
} TaskPool;

typedef void RegisterTask(void *, Handle);
typedef void FinishTask(void *, Handle *, Handle *);
typedef void FreeTask(void *, Handle);
typedef State PollTask(void *, Handle pollee);
typedef Handle CurrentTask(void *);
typedef Handle NextTask(void *);
typedef void Cleanup(void *);

typedef struct {
  RegisterTask *register_task;
  FinishTask *finish_task;
  FreeTask *free_task;
  PollTask *poll_task;
  CurrentTask *current_task;
  NextTask *next_task;
  Cleanup *cleanup;
} SchedulerVTable;

typedef struct {
  void *data;
  SchedulerVTable *vtable;
} Scheduler;

Scheduler *global_scheduler();
TaskPool *global_pool();

void use_queue_scheduler();
void async_init();
Handle start_new_task(AsyncFunction *fn, void *data);
void free_task(Handle h);
Task *get_task(Handle h);
State poll_state(Handle h);
void finish_current_task(Handle *finished_task, Handle *next_task);
Handle current_task_handle();
Handle next_task_handle();
void async_deinit();

#endif
