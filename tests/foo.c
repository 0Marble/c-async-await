/*
!!./build/tests/foo
$$gcc
$$./tests/foo.c
$$-o
$$./build/tests/foo
##Hello World!
 */

#include <stdio.h>
int main(int argc, char *argv[]) {
  printf("Hello World!");
  return 0;
}
