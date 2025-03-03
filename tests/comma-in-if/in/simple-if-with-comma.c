#include <stdio.h>

int azaza(int A, int B) {
	printf("%d", A);
	printf("%d", B);
	return 1;
}

int main(int argc, char **argv) {
	int X = 1;
	if ((X = 2, X < 10)) {
		printf("%d", X);
	}
	
}
