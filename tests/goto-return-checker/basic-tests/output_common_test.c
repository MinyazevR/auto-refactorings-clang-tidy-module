#include <stdio.h>

int lol(int y) {
	if (y >123) {
		goto LAB3;
	}
	return 1;
LAB3:
	printf("%d", 1);
}

int main(int argc, char **argv) {
	if (argc > 5) {
		return 123;
	}
	return 456;
LAB1:
	return 123;
LAB2:
	return 456;
}
