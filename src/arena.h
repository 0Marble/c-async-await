#ifndef __ARENA_H__
#define __ARENA_H__

typedef struct Arena {
  struct Arena *next;
  void *elems;
  int len;
  int cap;
} Arena;

Arena *arena_init();
void arena_deinit(Arena *a);
void *arena_alloc(Arena *a, int elem_cnt, int elem_size);

#endif // !__ARENA_H__
