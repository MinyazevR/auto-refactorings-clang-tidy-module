#include <stdio.h>

int main(int argc, char** argv) {
  if(argc > 5) {
    printf("%d", 1);
    if(argc > 7) {
      int z = 123;
      int u = 345;
    } else {
      int x;
      int y;
      printf("%d", x + y);
    } 
  } else {
    printf("%d", 2);
    printf("%d", 3);
  }
  return 228;
}
