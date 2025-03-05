#include <stdio.h>

int lol() {
	return 0;
}

int kek(int u) {
	return 0 + u;
}

int main(int argc, char** argv) {
	if(lol() && kek(lol())) {
		printf("%d", 1);
	}
}
