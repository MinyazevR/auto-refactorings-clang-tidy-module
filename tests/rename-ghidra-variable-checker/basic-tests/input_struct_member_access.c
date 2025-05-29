#include <stdio.h>
#include "basic_test.h"

int main(int argc, char** argv) {
  pack_test_struct_t t;
  t._0_1 = 'c';
  t._1_4 = 123;
  
  pack_test_struct_t* y;
  y->_0_1 = 'c';
  y->_1_4 = 123;
  
  test_struct_t z;
  z._0_1 = 'c';
  z._4_4 = 123;
  
  test_struct_t* u;
  u->_0_1 = 'c';
  u->_4_4 = 123;
}
