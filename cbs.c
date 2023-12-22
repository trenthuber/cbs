#define CBS_IMPLEMENTATION
#include "cbs.h"

int main(int argc, char **argv) {
	cbs_rebuild_self(argc, argv);
	cbs_shift_args(&argc, &argv);

	Cbs_Pids pids = {0};

	char *current_arg;
	while ((current_arg = cbs_shift_args(&argc, &argv))) {
		if (cbs_str_eq(current_arg, "build")) {
			cbs_run("mkdir", "-p", "build");
			if (cbs_needs_rebuild("./build/hello", "hello.c")) {
				cbs_pids_append(&pids, cbs_run_async("cc", "-o", "./build/hello", "hello.c"));
			}
		} else if(cbs_str_eq(current_arg, "run")) {
			if (cbs_file_exists("./build/hello"))
				cbs_run("./build/hello");
			else
				CBS_LOG("File not found, nothing to run");
		} else if(cbs_str_eq(current_arg, "clean")) {
			cbs_run("rm", "-rf", "build");
		} else {
			CBS_ERROR("Not a valid argument");
		}
	}
	cbs_pids_wait(&pids);

	return 0;
}
