#include "storage.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

Handle dequeue(Queue *q) {
  if (q->pop.len == 0) {
    int len = q->push.len;
    for (int i = 0; i < len; i++) {
      push_stack(&q->pop, pop_stack(&q->push));
    }
  }
  return pop_stack(&q->pop);
}

void enqueue(Queue *q, Handle h) { push_stack(&q->push, h); }

Handle peek_queue(Queue *q) {
  if (q->pop.len != 0) {
    return peek_stack(&q->pop);
  } else {
    assert(q->push.len > 0);
    return q->push.elems[0];
  }
}

Handle new_handle(FunctionStore *store, FunctionObject f) {
  if (store->next_free == 0) {
    DBG("Creating new function object", 0);
    f.this_fn = (Handle){.idx = store->count + 1};
    if (store->count >= store->cap) {
      store->cap = store->cap * 2 + 10;
      store->elems = realloc(store->elems, store->cap * sizeof(FunctionObject));
      assert(store->elems);
    }
    store->elems[store->count] = f;
    store->count++;
    return f.this_fn;
  } else {
    DBG("Reusing function object %d", store->next_free);
    f.this_fn = (Handle){.idx = store->next_free};
    FunctionObject *old = &store->elems[store->next_free - 1];
    assert(old->state == DEAD);
    int next_free = old->this_fn.idx;
    *old = f;
    store->next_free = next_free;
    return f.this_fn;
  }
}

void free_handle(FunctionStore *store, Handle h) {
  assert(h.idx != 0);
  assert(h.idx != 1 && "Dont free async_main");

  FunctionObject *f = &store->elems[h.idx - 1];
  assert(f->state != DEAD && "Double free!");

  int next_free = f->this_fn.idx;
  f->state = DEAD;
  f->this_fn.idx = store->next_free;
  f->stack_top = f->stack + STACK_SIZE;
  store->next_free = next_free;
}

void deinit_store(FunctionStore *store) {
  for (int i = 0; i < store->count; i++) {
    FunctionObject *f = &store->elems[i];
    if (f->stack && munmap(f->stack, STACK_SIZE) == -1) {
      perror("munmap: ");
    }
  }
  if (store->cap != 0) {
    free(store->elems);
  }
}

Handle pop_stack(Stack *s) {
  assert(s->len >= 0);
  s->len--;
  return s->elems[s->len];
}
void push_stack(Stack *s, Handle h) {
  if (s->len >= s->cap) {
    s->cap = s->cap * 2 + 10;
    s->elems = realloc(s->elems, s->cap * sizeof(Handle));
  }
  s->elems[s->len++] = h;
}
Handle peek_stack(Stack *s) {
  assert(s->len >= 0);
  return s->elems[s->len - 1];
}
void deinit_stack(Stack *s) {
  if (s->cap != 0)
    free(s->elems);
  s->cap = 0;
  s->len = 0;
  s->elems = NULL;
}
