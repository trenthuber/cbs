#define CBS_IMPLEMENTATION
#include "cbs.h"

int main(int argc, char **argv) {
	cbs_rebuild_self(argc, argv);

	cbs_run("mkdir", "test");
	cbs_run("ls");
	cbs_run("rmdir", "test");
	cbs_run("ls");

	return 0;
}
