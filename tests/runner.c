#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
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
      fprintf(stderr, "string invalid: `%*s', len=%d, cap=%d\n", s->len,       \
              s->str, s->len, s->cap);                                         \
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

void string_concat(String *a, const String *b) {
  STRING_OK(a);
  STRING_OK(b);
  int a_len = a->len + 1;
  int b_len = b->len + 1;

  int new_cap = a->cap;
  while (a_len + b_len >= new_cap) {
    new_cap = new_cap * 2 + 10;
  }
  if (a->cap != new_cap) {
    a->str = realloc(a->str, new_cap);
    a->cap = new_cap;
  }
  memcpy(&a->str[a->len], b->str, b->len);
  a->len += b->len;
  a->str[a->len] = '\0';
  STRING_OK(a);
}

void string_concat_with_cstr(String *a, const char *cstr) {
  int len = strlen(cstr);
  String cstr_as_string =
      (String){.str = (char *)cstr, .len = len, .cap = len + 1};
  string_concat(a, &cstr_as_string);
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

StringRef string_ref_next_token(StringRef str, StringRef *cur_tok) {
  const char *start = 0;
  if (cur_tok != NULL) {
    start = cur_tok->str;
  }

  int len = 0;
  while (isspace(*start)) {
    start++;
  }
  while (!isspace(start[len]) && start + len <= str.str + str.len) {
    len++;
  }

  return (StringRef){.str = start, len = len};
}

void string_ref_clone(StringRef ref, String *out) {
  for (int i = 0; i < ref.len; i++)
    string_append(out, ref.str[i]);
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

void parse_file(const String *file_path, Command *compile_command,
                Command *run_command, RunList *runs) {
  typedef enum {
    Start,
    ReadOneSpecial,
    InSpecial,
    NewLineAfterSpecial,
    ReadOneSpecialAfterNewline,
  } State;

  FILE *f = fopen(file_path->str, "r");
  if (!f) {
    perror("fopen");
    exit(1);
  }

  int c = 0;
  char special = '\0';
  State state = Start;

  assert(runs);
  RunList *input_ptr = NULL;
  RunList *output_ptr = NULL;
  String arg;
  string_init(&arg);

  while ((c = fgetc(f)) != EOF) {
  restart:
    switch (state) {
    case Start: {
      special = '\0';
      if (c == '$' || c == '#' || c == '%' || c == '!') {
        state = ReadOneSpecial;
        special = c;
      }
    } break;
    case ReadOneSpecial: {
      if (c == special) {
        state = InSpecial;

        if (special == '%') {
          if (input_ptr == NULL) {
            input_ptr = runs;
          } else if (input_ptr->next) {
            input_ptr = input_ptr->next;
          } else {
            RunList *next_node = run_list_init();
            input_ptr->next = next_node;
            input_ptr = next_node;
          }
        } else if (special == '#') {
          if (output_ptr == NULL) {
            output_ptr = runs;
          } else if (output_ptr->next) {
            output_ptr = input_ptr->next;
          } else {
            RunList *next_node = run_list_init();
            output_ptr->next = next_node;
            output_ptr = next_node;
          }
        }
      } else {
        state = Start;
      }
    } break;
    case InSpecial: {
      if (c == '\n') {
        state = NewLineAfterSpecial;
        if (special == '$') {
          command_append(compile_command, string_ref(&arg));
          string_clear(&arg);
        } else if (special == '!') {
          LOG("run arg: `%s'", arg.str);
          command_append(run_command, string_ref(&arg));
          string_clear(&arg);
        }
      } else if (special == '$' || special == '!') {
        string_append(&arg, c);
      } else if (special == '%') {
        string_append(&input_ptr->input, c);
      } else if (special == '#') {
        string_append(&output_ptr->output, c);
      }
    } break;
    case NewLineAfterSpecial: {
      if (c == special) {
        state = ReadOneSpecialAfterNewline;
      } else {
        state = Start;
        goto restart;
      }
    } break;
    case ReadOneSpecialAfterNewline: {
      if (c == special) {
        state = InSpecial;
        if (special == '%') {
          string_append(&input_ptr->input, '\n');
        } else if (special == '#') {
          string_append(&output_ptr->output, '\n');
        }
      } else {
        state = Start;
        goto restart;
      }
    } break;
    }
  }
  command_finish(compile_command);
  command_finish(run_command);

  fclose(f);
}

typedef int ForkFn(void **args);
void run_in_fork(ForkFn *fork_fn, void **args, StringRef fork_input,
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
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);
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

int dummy(void **args) { return (int)(long)(args); }

int main(int argc, char *argv[]) {
  assert(argc == 2);
  const char *test_file_name = argv[1];
  const char *tests_dir = "tests";
  const char *build_dir = "build/tests";
  const char *test_ext = ".c";

  LOG("Preparing test `%s'...", test_file_name);

  String test_file_path;
  string_init(&test_file_path);
  string_concat_with_cstr(&test_file_path, tests_dir);
  string_append(&test_file_path, '/');
  string_concat_with_cstr(&test_file_path, test_file_name);
  string_concat_with_cstr(&test_file_path, test_ext);

  LOG("Test source file: `%s'", test_file_path.str);

  Command compile_command, run_command;
  command_init(&compile_command);
  command_init(&run_command);
  RunList *run_list = run_list_init();
  LOG("Parsing test setup for `%s'...", test_file_name);
  parse_file(&test_file_path, &compile_command, &run_command, run_list);
  LOG("Parsed test setup for `%s'", test_file_name);

  void *compile_args[] = {&compile_command};
  String fork_output;
  string_init(&fork_output);
  run_in_fork(command_execute, compile_args, (StringRef){0}, &fork_output);
  LOG("Compiled `%s': `%s'", test_file_name, fork_output.str);
  string_clear(&fork_output);

  LOG("Running `%s'", test_file_name);
  RunList *current_run = run_list;
  for (int i = 0; current_run != NULL; i++, current_run = current_run->next) {
    LOG("Run %d for `%s'", i, test_file_name);
    void *run_args[] = {&run_command};
    string_clear(&fork_output);
    run_in_fork(command_execute, run_args, string_ref(&current_run->input),
                &fork_output);
    if (fork_output.len != current_run->output.len ||
        strcmp(fork_output.str, current_run->output.str) != 0) {
      LOG("Run %d for `%s': fail, expected `%s', got `%s'", i, test_file_name,
          current_run->output.str, fork_output.str);
    } else {
      LOG("Run %d for `%s': ok", i, test_file_name);
    }
  }

  run_list_deinit(run_list);
  command_deinit(&compile_command);
  command_deinit(&run_command);
  string_deinit(&test_file_path);
  string_deinit(&fork_output);

  return 0;
}
