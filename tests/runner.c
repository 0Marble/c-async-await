#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef STRING_OK
#define STRING_OK(s)                                                           \
  do {                                                                         \
    if ((s) && ((s)->cap == 0 && (s)->len == 0 && (s)->str == NULL) ||         \
        ((s)->cap >= (s)->len + 1 && s->str && s->str[s->len] == '\0')) {      \
    } else {                                                                   \
      LOG("string invalid: `%.*s', len=%d, cap=%d\n", s->len, s->str, s->len,  \
          s->cap);                                                             \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
#endif

#ifndef LOG
#define LOG(fmt, ...)                                                          \
  do {                                                                         \
    fprintf(stderr, "[%s:%d] " fmt "\n", __FILE_NAME__, __LINE__,              \
            __VA_ARGS__);                                                      \
  } while (0)
#endif

typedef struct {
  char *str;
  int len;
  int cap;
} String;

void string_init(String *str) { *str = (String){0}; }
void string_deinit(String *str) {
  if (str->cap != 0) {
    free(str->str);
  }
  *str = (String){0};
}

void string_append(String *str, char c) {
  STRING_OK(str);
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
  STRING_OK(out);
  assert(cstr);
  int len = strlen(cstr);
  out->len = len;
  out->cap = len + 1;
  out->str = realloc(out->str, out->cap);
  assert(out->str);
  memcpy(out->str, cstr, out->len);
  out->str[out->len] = '\0';
  STRING_OK(out);
}

void string_clear(String *str) {
  STRING_OK(str);
  str->len = 0;
  if (str->cap != 0)
    str->str[0] = '\0';
}

void string_clone(String *src, String *dst) {
  STRING_OK(src);
  STRING_OK(dst);
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
  STRING_OK(dst);
}

typedef struct {
  const char *str;
  int len;
} StringRef;

StringRef string_ref(String *s) {
  STRING_OK(s);
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
  STRING_OK(a);
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
  STRING_OK(a);
}

void string_concat_with_cstr(String *a, const char *cstr) {
  string_concat(a, string_ref_from_cstr(cstr));
}

typedef struct RunList {
  String input, output;
  struct RunList *next;
} RunList;

RunList *run_list_init() {
  RunList *node = malloc(sizeof(RunList));
  assert(node);
  string_init(&node->input);
  string_init(&node->output);
  node->next = NULL;
  return node;
}

void run_list_deinit(RunList *run_list) {
  for (RunList *l = run_list; l != NULL;) {
    RunList *next = l->next;
    string_deinit(&l->input);
    string_deinit(&l->output);
    free(l);
    l = next;
  }
}

typedef struct {
  char **args;
  int len;
  int cap;
} Command;

void command_init(Command *cmd) { *cmd = (Command){0}; }

void command_deinit(Command *cmd) {
  if (cmd->cap != 0) {
    for (int i = 0; i + 1 < cmd->len; i++) {
      free(cmd->args[i]);
    }
    free(cmd->args);
  }
  *cmd = (Command){0};
}

void command_append(Command *cmd, StringRef arg) {
  if (cmd->len + 1 >= cmd->cap) {
    cmd->cap = cmd->cap * 2 + 10;
    cmd->args = realloc(cmd->args, cmd->cap * sizeof(char *));
    assert(cmd->args);
  }
  String clone;
  string_init(&clone);
  string_ref_clone(arg, &clone);
  cmd->args[cmd->len] = clone.str;
  cmd->len++;
}

void command_finish(Command *cmd) {
  if (cmd->len + 1 >= cmd->cap) {
    cmd->cap = cmd->cap * 2 + 10;
    cmd->args = realloc(cmd->args, cmd->cap * sizeof(char *));
    assert(cmd->args);
  }
  cmd->args[cmd->len] = NULL;
  cmd->len++;
}

bool string_read_line(FILE *fptr, String *out) {
  STRING_OK(out);
  int c = 0;
  while ((c = fgetc(fptr)) != EOF) {
    if (c == '\n')
      break;
    string_append(out, c);
  }
  return c != EOF;
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

void parse_file(const String *file_path, Command *compile_command,
                Command *run_command, RunList *runs) {
  typedef enum {
    Normal,
    Compile,
    Run,
    Input,
    Output,
    Split,
  } LastLineKind;

  FILE *f = fopen(file_path->str, "r");
  if (!f) {
    perror("fopen");
    exit(1);
  }

  LastLineKind last_line_kind = Normal;
  int line_num = 0;
  String line, input, output;
  string_init(&line);
  string_init(&input);
  string_init(&output);

  RunList *this_run = NULL;

  while (string_read_line(f, &line)) {
    StringRef l = string_ref(&line);
    line_num += 1;
    string_ref_trim_left_ws(&l);

    StringRef compile_line = l, run_line = l, input_line = l, output_line = l,
              split_line = l;
    if (string_ref_trim_left(&compile_line, '$') == 2) {
      string_ref_trim_left_ws(&compile_line);
      StringRef arg = compile_line;
      while (string_ref_split_ws_once(&compile_line, &arg)) {
        command_append(compile_command, arg);
      }

      last_line_kind = Compile;
    } else if (string_ref_trim_left(&run_line, '!') == 2) {
      string_ref_trim_left_ws(&run_line);
      StringRef arg = run_line;
      while (string_ref_split_ws_once(&run_line, &arg)) {
        command_append(run_command, arg);
      }

      last_line_kind = Run;
    } else if (string_ref_trim_left(&output_line, '#') == 2) {
      // output line
      string_ref_trim_left_ws(&output_line);
      if (last_line_kind == Output) {
        string_append(&output, '\n');
      }
      string_concat(&output, output_line);

      last_line_kind = Output;
    } else if (string_ref_trim_left(&input_line, '%') == 2) {
      // input line
      string_ref_trim_left_ws(&input_line);
      if (last_line_kind == Input) {
        string_append(&input, '\n');
      }
      string_concat(&input, input_line);

      last_line_kind = Input;
    } else {
      if (string_ref_trim_left(&split_line, '-') != 0 && split_line.len == 0) {
        if (this_run == NULL) {
          this_run = runs;
        } else {
          runs->next = this_run;
          runs = this_run;
        }
        string_clone(&input, &this_run->input);
        string_clone(&output, &this_run->output);

        string_clear(&input);
        string_clear(&output);

        this_run = run_list_init();

        last_line_kind = Split;
      } else {
        last_line_kind = Normal;
      }
    }

    string_clear(&line);
  }

  string_deinit(&line);
  string_deinit(&input);
  string_deinit(&output);
  if (this_run && this_run != runs) {
    run_list_deinit(this_run);
  }

  command_finish(compile_command);
  command_finish(run_command);

  fclose(f);
}

typedef int ForkFn(void **args);
int run_in_fork(ForkFn *fork_fn, void **args, StringRef fork_input,
                String *fork_output) {

  int stdin_pipe[2] = {0};
  int stdout_pipe[2] = {0};
  if (pipe(stdin_pipe) < 0 || pipe(stdout_pipe) < 0) {
    perror("pipe");
    exit(1);
  }

  int pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(1);
  }

  if (pid == 0) {
    close(stdin_pipe[1]);
    close(stdout_pipe[0]);
    dup2(stdin_pipe[0], STDIN_FILENO);
    close(stdin_pipe[0]);
    dup2(stdout_pipe[1], STDOUT_FILENO);
    close(stdout_pipe[1]);

    if (fork_fn(args) != 0) {
      exit(1);
    } else {
      exit(0);
    }
  } else {
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    if (fork_input.len != 0) {
      write(stdin_pipe[1], fork_input.str, fork_input.len);
    }
    close(stdin_pipe[1]);

    if (fork_output) {
      while (true) {
        char buffer[1024] = {0};
        int read_amt = read(stdout_pipe[0], buffer, 1023);
        if (read_amt < 0) {
          perror("read");
          exit(1);
        }
        string_concat_with_cstr(fork_output, buffer);
        if (read_amt < 1023) {
          break;
        }
      }
    }

    int status = 0;
    waitpid(pid, &status, 0);
    if (!WIFEXITED(status))
      return -1;
    if (WEXITSTATUS(status) != 0)
      return -1;
    return 0;
  }
}

int command_execute(void **arg) {
  Command *cmd = arg[0];
  assert(cmd->len >= 2);

  String str;
  string_init(&str);
  for (int i = 0; i + 1 < cmd->len; i++) {
    string_append(&str, '`');
    string_concat_with_cstr(&str, cmd->args[i]);
    string_append(&str, '\'');
    string_append(&str, ' ');
  }
  LOG("Running command `%s'", str.str);
  string_deinit(&str);

  if (execvp(cmd->args[0], cmd->args) < 0) {
    perror("execvp");
    return 1;
  }

  return 0;
}

int run_test(const char *test_file_name) {
  const char *tests_dir = "tests";
  const char *build_dir = "build/tests";

  LOG("Preparing test `%s'...", test_file_name);

  String test_file_path;
  string_init(&test_file_path);
  string_concat_with_cstr(&test_file_path, tests_dir);
  string_append(&test_file_path, '/');
  string_concat_with_cstr(&test_file_path, test_file_name);

  LOG("Test source file: `%s'", test_file_path.str);

  Command compile_command, run_command;
  command_init(&compile_command);
  command_init(&run_command);
  RunList *run_list = run_list_init();
  LOG("Parsing test setup for `%s'...", test_file_name);
  parse_file(&test_file_path, &compile_command, &run_command, run_list);
  LOG("Parsed test setup for `%s'", test_file_name);

  String fork_output;
  string_init(&fork_output);
  void *compile_args[] = {&compile_command};
  if (run_in_fork(command_execute, compile_args, (StringRef){0},
                  &fork_output) != 0) {
    LOG("Compile for `%s' failed", test_file_name);
    return -1;
  }
  LOG("Compiled `%s': `%s'", test_file_name, fork_output.str);
  string_clear(&fork_output);

  LOG("Running `%s'", test_file_name);
  RunList *current_run = run_list;
  int success = 0;
  for (int i = 0; current_run != NULL; i++, current_run = current_run->next) {
    LOG("Run %d for `%s'", i, test_file_name);
    void *run_args[] = {&run_command};
    string_clear(&fork_output);
    if (run_in_fork(command_execute, run_args, string_ref(&current_run->input),
                    &fork_output) != 0) {
      LOG("Run %d for `%s' crashed", i, test_file_name);
      success = -1;
    } else if (fork_output.len != current_run->output.len ||
               strcmp(fork_output.str, current_run->output.str) != 0) {
      LOG("Run %d for `%s': fail, expected `%s', got `%s'", i, test_file_name,
          current_run->output.str, fork_output.str);
      success = -1;
    } else {
      LOG("Run %d for `%s': ok", i, test_file_name);
    }
  }

  run_list_deinit(run_list);
  command_deinit(&compile_command);
  command_deinit(&run_command);
  string_deinit(&test_file_path);
  string_deinit(&fork_output);

  return success;
}

int main(int argc, char *argv[]) {
  assert(argc == 2);

  bool success = true;
  String failed_tests;
  string_init(&failed_tests);

  if (strcmp(argv[1], "-") == 0) {
    String test_file_name;
    string_init(&test_file_name);

    while (string_read_line(stdin, &test_file_name)) {
      if (run_test(test_file_name.str) != 0) {
        success = false;
        string_append(&failed_tests, '`');
        string_concat(&failed_tests, string_ref(&test_file_name));
        string_concat_with_cstr(&failed_tests, "', ");
      }
      string_clear(&test_file_name);
    }

    string_deinit(&test_file_name);
  } else {
    if (run_test(argv[1]) != 0)
      success = false;
  }
  if (success) {
    LOG("%s", "All Ok!");
  } else {
    LOG("Had fails in tests: %s", failed_tests.str);
  }
  string_deinit(&failed_tests);

  return 0;
}
