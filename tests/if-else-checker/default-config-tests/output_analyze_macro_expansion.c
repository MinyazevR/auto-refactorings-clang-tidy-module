#include <stdio.h>

#define CHECK(x, y) if (x >= y)

int main(int argc, char** argv) {
  CHECK(argc, 5) {
    printf("%d", 2);
    printf("%d", 3);
  } else {
    printf("%d", 1);
  }
}
