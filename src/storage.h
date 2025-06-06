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

#define __queue_next(x, cap)                                                   \
  do {                                                                         \
    x = (x + 1) % cap;                                                         \
  } while (0)

#define __queue_prev(x, cap)                                                   \
  do {                                                                         \
    if (x == 0)                                                                \
      x = cap - 1;                                                             \
    else                                                                       \
      x--;                                                                     \
  } while (0)

#define __queue_expand(q)                                                      \
  do {                                                                         \
    int new_cap = (q)->cap * 2 + 10;                                           \
    (q)->elems = realloc((q)->elems, new_cap * sizeof((q)->elems[0]));         \
    assert((q)->elems);                                                        \
    assert((q)->cap + (q)->end <= new_cap);                                    \
    if ((q)->end < (q)->start) {                                               \
      memmove((q)->elems + (q)->cap, (q)->elems,                               \
              (q)->end * sizeof((q)->elems[0]));                               \
      (q)->end = ((q)->cap + (q)->end) % new_cap;                              \
    }                                                                          \
    (q)->cap = new_cap;                                                        \
  } while (0)

#define queue_push_back(q, elem)                                               \
  do {                                                                         \
    if ((q)->cap == 0) {                                                       \
      __queue_expand((q));                                                     \
    }                                                                          \
    if (((q)->end + 1) % (q)->cap == (q)->start) {                             \
      __queue_expand((q));                                                     \
    }                                                                          \
    (q)->elems[(q)->end] = (elem);                                             \
    __queue_next((q)->end, (q)->cap);                                          \
  } while (0)

#define queue_push_front(q, elem)                                              \
  do {                                                                         \
    if ((q)->cap == 0) {                                                       \
      __queue_expand((q));                                                     \
    }                                                                          \
    int next_start = (q)->start;                                               \
    __queue_prev(next_start, (q)->cap);                                        \
    if (next_start == (q)->end) {                                              \
      __queue_expand((q));                                                     \
    }                                                                          \
    __queue_prev((q)->start, (q)->cap);                                        \
    (q)->elems[(q)->start] = (elem);                                           \
  } while (0)

#define queue_pop_back(q, elem)                                                \
  do {                                                                         \
    assert((q)->end != (q)->start && "Underflow");                             \
    __queue_prev((q)->end, (q)->cap);                                          \
    *(elem) = (q)->elems[(q)->end];                                            \
  } while (0)

#define queue_pop_front(q, elem)                                               \
  do {                                                                         \
    assert((q)->start != (q)->end && "Underflow");                             \
    *(elem) = (q)->elems[(q)->start];                                          \
    __queue_next((q)->start, (q)->cap);                                        \
  } while (0)

#define queue_peek_front(q, elem)                                              \
  do {                                                                         \
    assert((q)->start != (q)->end && "Underflow");                             \
    *(elem) = (q)->elems[(q)->start];                                          \
  } while (0)

#endif // !__STORAGE_H__
