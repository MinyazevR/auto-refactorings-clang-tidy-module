#include <stdio.h>

#define CHECK(x, y) (x >= y)

int main(int argc, char** argv) {
  if(!(CHECK(argc, 5))) {
    printf("%d", 2);
    printf("%d", 3);
  } else {
    printf("%d", 1);
  }
}
