#ifndef STR_H
#define STR_H

#include <stdbool.h>

typedef struct {
  char *str;
  int len;
  int cap;
} String;

typedef struct {
  const char *str;
  int len;
} StringRef;

void string_init(String *str);
void string_deinit(String *str);
void string_append(String *str, char c);
void string_from_cstr(String *out, const char *cstr);
void string_clear(String *str);
void string_clone(String *src, String *dst);
StringRef string_ref(String *s);
StringRef string_ref_from_cstr(const char *str);
void string_ref_clone(StringRef ref, String *out);
void string_concat(String *a, StringRef b);
void string_concat_with_cstr(String *a, const char *cstr);
int string_ref_trim_left(StringRef *ref, char c);
int string_ref_trim_left_ws(StringRef *ref);
bool string_ref_split_ws_once(StringRef *original, StringRef *token);
StringRef string_substr(String *s, int start, int len);

#endif
