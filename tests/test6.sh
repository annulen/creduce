#!/bin/bash

rm -f out*.txt

ulimit -t 25
ulimit -v 6000000

if 
  clang -pedantic -Wall -O0 small.c  >out.txt 2>&1 &&\
  ! grep 'conversions than data arguments' out.txt &&\
  ! grep 'incompatible redeclaration' out.txt &&\
  ! grep 'ordered comparison between pointer' out.txt &&\
  ! grep 'eliding middle term' out.txt &&\
  ! grep 'end of non-void function' out.txt &&\
  ! grep 'invalid in C99' out.txt &&\
  ! grep 'specifies type' out.txt &&\
  ! grep 'should return a value' out.txt &&\
  ! grep 'uninitialized' out.txt &&\
  ! grep 'incompatible pointer to' out.txt &&\
  ! grep 'incompatible integer to' out.txt &&\
  ! grep 'type specifier missing' out.txt &&\
  gcc -Wall -Wextra -O small.c -o smallz >outa.txt 2>&1 &&\
  ! grep uninitialized outa.txt &&\
  ! grep 'without a cast' outa.txt &&\
  ! grep 'control reaches end' outa.txt &&\
  ! grep 'return type defaults' outa.txt &&\
  ! grep 'cast from pointer to integer' outa.txt &&\
  ! grep 'useless type name in empty declaration' outa.txt &&\
  ! grep 'no semicolon at end' outa.txt &&\
  ! grep 'type defaults to' outa.txt &&\
  ! grep 'too few arguments for format' outa.txt &&\
  ! grep 'incompatible pointer' outa.txt &&\
  ! grep 'ordered comparison of pointer with integer' outa.txt &&\
  ! grep 'declaration does not declare anything' outa.txt &&\
  ! grep 'expects type' outa.txt &&\
  ! grep 'pointer from integer' outa.txt &&\
  ! grep 'incompatible implicit' outa.txt &&\
  ! grep 'excess elements in struct initializer' outa.txt &&\
  ! grep 'comparison between pointer and integer' outa.txt &&\
  ./smallz >out1.txt 2>&1 &&\
  grep 'checksum = e' out1.txt &&\
  kcc small.c -o out-kcc >/dev/null 2>&1 &&\
  ./out-kcc
then
  exit 0
else
  exit 1
fi
