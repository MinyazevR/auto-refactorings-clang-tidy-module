#include <stdio.h>

int lol() {
	return 0;
}

int main(int argc, char** argv) {
	int x = 1;
	if(x > 1) {
		printf("%d", 1);
	} else if (lol()) {
		printf("%d", 2);
	} else {
		printf("%d", 3);
	}
}
