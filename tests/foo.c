/*
!! ./build/tests/foo
$$ gcc ./tests/foo.c -o ./build/tests/foo

%% 10
## 10
##
-----------

%% 20
## 20
##
-----------

%% -69
## -69
##
-----------
 */

#include <stdio.h>

int main(int argc, char *argv[]) {
  int number = 0;
  scanf("%d", &number);
  printf("%d\n", number);
  return 0;
}
