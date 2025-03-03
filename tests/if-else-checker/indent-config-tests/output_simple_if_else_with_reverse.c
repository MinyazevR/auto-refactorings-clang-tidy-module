#include <stdio.h>

int main(int argc, char** argv) {
  if(!(argc > 5)) {
    printf("%d", 1);
    return 0;
  } 

  printf("%d", 2);
  printf("%d", 3);
  return 0;
}
