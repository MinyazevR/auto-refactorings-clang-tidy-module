#include <stdio.h>
#include <stdlib.h> 

int *lol(int size) {
	return malloc(size * sizeof(int));
}

int kek() {
	return 5;
}

double *ahaha() {
	return malloc(sizeof(double));
}

double uhuhu() {
	return 5.0;
}


int main(int argc, char** argv) {

	int *x;
	double *y;
	
	x = lol(10);
	if(x != NULL)
		printf("%d", 1);

	if (kek()) {
		printf("%d", 2);
	}
	
	y = ahaha();
	if (y != NULL) {
		printf("%d", 3);
	}
	
	if (uhuhu()) {
		printf("%d", 4);
	}
}
