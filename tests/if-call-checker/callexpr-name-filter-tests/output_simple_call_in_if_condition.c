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
	int * var0 = functionPrefix1(10);
	if(var0)
		printf("%d", 1);

	if (kek()) {
		printf("%d", 2);
	}
	
	double * var1 = functionPrefix2();
	if (var1) {
		printf("%d", 2);
	}
	
	if (uhuhu()) {
		printf("%d", 2);
	}
}
