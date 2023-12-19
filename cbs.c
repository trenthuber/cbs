#include <stdio.h>

#define CBS_IMPLEMENTATION
#include "cbs.h"

int main(int argc, char **argv) {
	cbs_rebuild(argc, argv);

	cbs_run("mkdir", "test");
	cbs_run("rmdir", "test");

	printf("Hello world!\n");

	return 0;
}
