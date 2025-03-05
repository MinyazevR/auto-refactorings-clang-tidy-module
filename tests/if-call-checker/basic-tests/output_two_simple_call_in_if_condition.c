#include <stdio.h>

int lol() {
	return 0;
}

int main(int argc, char** argv) {
	int variable0 = lol();
	int variable1 = lol();
	if(variable0 && variable1) {
		printf("%d", 1);
	}
}
