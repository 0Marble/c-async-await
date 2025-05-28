#ifndef __STORAGE_H__
#define __STORAGE_H__

#include "async.h"

typedef struct {
  FunctionObject *elems;
  int count;
  int cap;
  int next_free;
} FunctionStore;

typedef struct {
  Handle *elems;
  int len;
  int cap;
} Stack;

typedef struct {
  Stack push, pop;
} Queue;

Handle pop_stack(Stack *s);
void push_stack(Stack *s, Handle h);
Handle peek_stack(Stack *s);
void deinit_stack(Stack *s);

Handle dequeue(Queue *q);
void enqueue(Queue *q, Handle h);
Handle peek_queue(Queue *q);

Handle new_handle(FunctionStore *store, FunctionObject f);
void free_handle(FunctionStore *store, Handle h);
void deinit_store(FunctionStore *store);

#endif
