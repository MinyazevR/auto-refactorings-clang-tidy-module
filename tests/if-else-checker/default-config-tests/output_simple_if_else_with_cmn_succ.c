#include <stdio.h>

int main(int argc, char** argv) {
  if(!(argc > 5)) {
    printf("%d", 1);
  } else {
    printf("%d", 2);
    printf("%d", 3);
  }
  printf("%d", 4);
}
