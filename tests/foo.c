/*
!! ./build/tests/foo
$$ gcc ./tests/foo.c -o ./build/tests/foo

%% 10
%% 20
## 30
##
-----------

%% -1
%% 2
## 1
##
-----------

%% 100
%% -31
## 69
##
-----------
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
  int a = 0, b = 0;
  scanf("%d", &a);
  scanf("%d", &b);
  printf("%d\n", a + b);
  return 0;
}
