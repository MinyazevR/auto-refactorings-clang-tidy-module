#include <stdio.h>
#include <string.h>

void run(char *buf) {
	char *begin_buf = buf;
	if (strncmp(buf, "null", 4) == 0) {
		printf("%d", 1);
     	}
}
