#include "str.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// #define BUF_SIZE 16 * 1024 * 1024
// char string_buf[BUF_SIZE] = {0};
// size_t buf_ptr = 0;
//
// static void free(void *ptr) {}
//
// static void *calloc(size_t n, size_t elem_size) {
//   assert(n > 0);
//   int byte_cnt = n * elem_size;
//   assert(buf_ptr + byte_cnt + 4 < BUF_SIZE);
//   char *size_ptr = string_buf + buf_ptr;
//   char *ptr = string_buf + buf_ptr + 4;
//   *(int *)size_ptr = byte_cnt;
//   memset(ptr, 0, byte_cnt);
//   buf_ptr += byte_cnt + 4;
//   return ptr;
// }
//
// static void *realloc(void *old, size_t new_size) {
//   if (old == NULL) {
//     return calloc(new_size, 1);
//   }
//   int old_size = *(int *)(old - 4);
//   if (new_size == old_size) {
//     return old;
//   }
//   char *new = calloc(new_size, 1);
//   assert(old_size > 0);
//   if (old_size < new_size) {
//     memcpy(new, old, old_size);
//   } else {
//     memcpy(new, old, new_size);
//   }
//   return new;
// }

void string_init(String *str) { *str = (String){0}; }
void string_deinit(String *str) {
  if (str->cap != 0) {
    free(str->str);
  }
  *str = (String){0};
}
void string_append(String *str, char c) {
  if (str->len + 2 >= str->cap) {
    str->cap = str->cap * 2 + 10;
    str->str = realloc(str->str, str->cap);
    assert(str->str);
  }
  str->str[str->len] = c;
  str->str[str->len + 1] = '\0';
  str->len++;
}

void string_from_cstr(String *out, const char *cstr) {
  assert(cstr);
  int len = strlen(cstr);
  out->len = len;
  out->cap = len + 1;
  out->str = realloc(out->str, out->cap);
  assert(out->str);
  memcpy(out->str, cstr, out->len);
  out->str[out->len] = '\0';
}

void string_clear(String *str) {
  str->len = 0;
  if (str->cap != 0)
    str->str[0] = '\0';
}

void string_clone(String *src, String *dst) {
  int new_cap = dst->cap;
  while (src->len + 2 >= new_cap) {
    new_cap = new_cap * 2 + 10;
  }
  if (new_cap != dst->cap) {
    dst->str = realloc(dst->str, new_cap);
    dst->cap = new_cap;
  }
  memcpy(dst->str, src->str, src->len + 1);
  dst->len = src->len;
}

StringRef string_ref(String *s) {
  return (StringRef){.str = s->str, .len = s->len};
}

StringRef string_ref_from_cstr(const char *str) {
  assert(str);
  int len = strlen(str);
  return (StringRef){.str = str, .len = len};
}

void string_ref_clone(StringRef ref, String *out) {
  for (int i = 0; i < ref.len; i++)
    string_append(out, ref.str[i]);
}

void string_concat(String *a, StringRef b) {
  int a_len = a->len + 1;
  int b_len = b.len + 1;

  int new_cap = a->cap;
  while (a_len + b_len >= new_cap) {
    new_cap = new_cap * 2 + 10;
  }
  if (a->cap != new_cap) {
    a->str = realloc(a->str, new_cap);
    a->cap = new_cap;
  }
  memcpy(&a->str[a->len], b.str, b.len);
  a->len += b.len;
  a->str[a->len] = '\0';
}

void string_concat_with_cstr(String *a, const char *cstr) {
  string_concat(a, string_ref_from_cstr(cstr));
}

int string_ref_trim_left(StringRef *ref, char c) {
  int i = 0;
  for (; i < ref->len; i++) {
    if (ref->str[i] != c)
      break;
  }
  ref->str = &ref->str[i];
  ref->len -= i;

  return i;
}

int string_ref_trim_left_ws(StringRef *ref) {
  int sum = 0;
  while (true) {
    int d = string_ref_trim_left(ref, ' ') + string_ref_trim_left(ref, '\t');
    sum += d;
    if (d == 0)
      break;
  }
  return sum;
}

bool string_ref_split_ws_once(StringRef *original, StringRef *token) {
  int i = 0;
  for (; i < original->len; i++) {
    if (!isspace(original->str[i]))
      break;
  }
  int start = i;

  for (; i < original->len; i++) {
    if (isspace(original->str[i]))
      break;
  }
  int end = i;
  int len = end - start;

  *token = (StringRef){
      .str = &original->str[start],
      .len = len,
  };
  StringRef old = *original;
  original->len = (original->str + original->len) - (token->str + token->len);
  original->str = token->str + token->len;
  return len != 0;
}

StringRef string_substr(String *s, int start, int len) {
  assert(start < s->len && start + len < s->len);
  return (StringRef){.str = &s->str[start], .len = len};
}

void string_reserve(String *s, int cap) {
  if (s->cap >= cap + 1)
    return;
  s->str = realloc(s->str, cap + 1);
}

void string_resize(String *s, int new_size, char pad_char) {
  if (s->len == new_size)
    return;
  else if (s->len > new_size) {
    s->len = new_size;
    if (s->cap != 0) {
      s->str[s->len] = '\0';
    }
  } else {
    s->cap = new_size + 1;
    s->str = realloc(s->str, s->cap);
    assert(s->str);
    memset(s->str + s->len, pad_char, s->cap);
    s->len = new_size;
    s->str[s->len] = '\0';
  }
}
