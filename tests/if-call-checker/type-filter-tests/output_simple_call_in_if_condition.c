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
	if(lol(10))
		printf("%d", 1);

	int conditionVariable0 = kek();
	if (conditionVariable0) {
		printf("%d", 2);
	}
	
	double * conditionVariable1 = ahaha();
	if (conditionVariable1) {
		printf("%d", 2);
	}
	
	if (uhuhu()) {
		printf("%d", 2);
	}
}
