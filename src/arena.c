#include "arena.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

Arena *arena_init() {
  Arena *ptr = calloc(1, sizeof(Arena));
  assert(ptr);
  ptr->cap = 64;
  ptr->elems = malloc(ptr->cap);
  assert(ptr->elems);
  return ptr;
}

void arena_deinit(Arena *a) {
  for (; a;) {
    Arena *next = a->next;
    if (a->cap)
      free(a->elems);
    free(a);
    a = next;
  }
}

void *arena_alloc(Arena *a, int elem_cnt, int elem_size) {
  for (; a;) {
    int size = elem_cnt * elem_size;
    if (a->len + size < a->cap) {
      void *ptr = a->elems + a->len;
      a->len += size;
      return ptr;
    } else if (a->next) {
      a = a->next;
    } else {
      Arena *new_arena = calloc(1, sizeof(Arena));
      assert(new_arena);
      new_arena->len = 0;
      new_arena->cap = a->cap * 2;
      new_arena->elems = malloc(a->cap);
      assert(new_arena->elems);
      a->next = new_arena;
      a = new_arena;
    }
  }
  assert(false);
}
