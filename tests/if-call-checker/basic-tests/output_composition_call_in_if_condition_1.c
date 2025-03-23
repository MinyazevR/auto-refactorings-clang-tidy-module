#include <stdio.h>

int lol() {
	return 0;
}

int kek(int u) {
	return 0 + u;
}

int main(int argc, char** argv) {
	int variable0 = kek(lol());
	if(variable0) {
		printf("%d", 1);
	}
}
