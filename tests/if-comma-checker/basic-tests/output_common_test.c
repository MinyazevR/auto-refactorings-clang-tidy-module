#include <stdio.h>

int lol() {
	return 5;
}

int main(int argc, char **argv) {
	int x;
	x = 5;
	if((x < 10)) {
		printf("%d", 1);
	}
	
	int y;
	int z;
	x = 6;
	y = 5;
	if((x + y) < 10) {
		printf("%d", 1);
	}
	
	z = 7;
	x = 6;
	y = 5;
	if(((x + y) - z < 10))
		printf("%d", 1);
		
	x = lol();
	y = 5;
	if(((x + y) < 10)) {
		printf("%d", 1);
	}
	
	int u;
	int t;
	x = lol();
	y = 5;
	u = 4;
	t = 228;
	if(((x + y) < 10)
		&& ((u - t) < 0)) {
		printf("%d", 1);
	}
}
