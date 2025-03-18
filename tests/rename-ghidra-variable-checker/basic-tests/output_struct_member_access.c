#include <stdio.h>
#include "basic_test.h"

int main(int argc, char** argv) {
  pack_test_struct_t t;
  t.member1 = 'c';
  t.member2 = 123;
  
  pack_test_struct_t* y;
  y->member1 = 'c';
  y->member2 = 123;
  
  test_struct_t z;
  z.member1 = 'c';
  z.member2 = 123;
  
  test_struct_t* u;
  u->member1 = 'c';
  u->member2 = 123;
}
