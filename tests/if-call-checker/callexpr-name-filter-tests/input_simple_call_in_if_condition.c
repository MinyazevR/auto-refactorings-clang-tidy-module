#include <stdio.h>
#include <stdlib.h> 

int *functionPrefix1(int size) {
	return malloc(size * sizeof(int));
}

int kek() {
	return 5;
}

double *functionPrefix2() {
	return malloc(sizeof(double));
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {
	if(functionPrefix1(10))
		printf("%d", 1);

	if (kek()) {
		printf("%d", 2);
	}
	
	if (functionPrefix2()) {
		printf("%d", 2);
	}
	
	if (uhuhu()) {
		printf("%d", 2);
	}
}
