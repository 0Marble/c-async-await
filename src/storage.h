#ifndef __STORAGE_H__
#define __STORAGE_H__

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define stack_push(stack, elem)                                                \
  do {                                                                         \
    if ((stack)->len == (stack)->cap) {                                        \
      (stack)->cap = (stack)->cap * 2 + 10;                                    \
      (stack)->elems = realloc((stack)->elems, (stack)->cap * sizeof(elem));   \
    }                                                                          \
    (stack)->elems[(stack)->len++] = (elem);                                   \
  } while (0)

#define stack_pop(stack, out)                                                  \
  do {                                                                         \
    assert((stack)->len);                                                      \
    *(out) = (stack)->elems[--(stack)->len];                                   \
  } while (0)

#define stack_peek(stack, out)                                                 \
  do {                                                                         \
    assert((stack)->len);                                                      \
    *(out) = (stack)->elems[(stack)->len - 1];                                 \
  } while (0)

#define queue_expand(q)                                                        \
  do {                                                                         \
    int len = (q)->end - (q)->start;                                           \
    (q)->cap = (q)->cap * 2 + 10;                                              \
    int new_start = ((q)->cap - len) / 2;                                      \
    (q)->elems = realloc((q)->elems, (q)->cap * sizeof((q)->elems[0]));        \
    memmove((q)->elems + new_start, (q)->elems + (q)->start,                   \
            len * sizeof((q)->elems[0]));                                      \
    (q)->start = new_start;                                                    \
    (q)->end = (q)->start + len;                                               \
  } while (0)

#define queue_push_back(q, elem)                                               \
  do {                                                                         \
    if ((q)->start == 0 || (q)->end == (q)->cap) {                             \
      queue_expand(q);                                                         \
    }                                                                          \
    assert((q)->end < (q)->cap);                                               \
    (q)->elems[(q)->end++] = elem;                                             \
  } while (0)

#define queue_push_front(q, elem)                                              \
  do {                                                                         \
    if ((q)->start == 0 || (q)->end == (q)->cap) {                             \
      queue_expand(q);                                                         \
    }                                                                          \
    assert((q)->start != 0);                                                   \
    (q)->elems[--(q)->start] = elem;                                           \
  } while (0)

#define queue_pop_front(q, out)                                                \
  do {                                                                         \
    assert((q)->start < (q)->end);                                             \
    *(out) = (q)->elems[(q)->start++];                                         \
  } while (0)

#define queue_peek_front(q, out)                                               \
  do {                                                                         \
    assert((q)->start < (q)->end);                                             \
    *(out) = (q)->elems[(q)->start];                                           \
  } while (0)

#define queue_pop_back(q, out)                                                 \
  do {                                                                         \
    assert((q)->start < (q)->end);                                             \
    *(out) = (q)->elems[--(q)->end];                                           \
  } while (0)

#define queue_peek_back(q, out)                                                \
  do {                                                                         \
    assert((q)->start < (q)->end);                                             \
    *(out) = (q)->elems[(q)->end - 1];                                         \
  } while (0)

#endif // !__STORAGE_H__
