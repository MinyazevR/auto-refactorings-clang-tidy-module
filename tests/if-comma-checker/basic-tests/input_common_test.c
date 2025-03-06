#include <stdio.h>

int lol() {
	return 5;
}

int main(int argc, char **argv) {
	int x;
	if((x = 5, x < 10)) {
		printf("%d", 1);
	}
	
	int y;
	int z;
	if((x = 6, y = 5, (x + y) < 10)) {
		printf("%d", 1);
	}
	
	if((z = 7, x = 6, y = 5, (x + y) - z < 10))
		printf("%d", 1);
		
	if((x = lol(), y = 5, (x + y) < 10)) {
		printf("%d", 1);
	}
	
	int u;
	int t;
	if((x = lol(), y = 5, (x + y) < 10)
		&& (u = 4, t = 228, (u - t) < 0)) {
		printf("%d", 1);
	}
}
