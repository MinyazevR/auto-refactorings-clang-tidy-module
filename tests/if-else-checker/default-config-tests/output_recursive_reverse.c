#include <stdio.h>

int main(int argc, char** argv) {
  if(!(argc > 5)) {
    printf("%d", 2);
    printf("%d", 3);
  } else {
    printf("%d", 1);
    if(!(argc > 7)) {
      int x;
      int y;
    } else {
      int z = 123;
      int u = 345;
      printf("%d", z + u);
    } 
  }
  return 228;
}
