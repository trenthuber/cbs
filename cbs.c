#define CBS_IMPLEMENTATION
#include "cbs.h"

#define CFLAGS "-Wall", "-Wextra", "-Wpedantic", "-std=c17"

int main(int argc, char **argv) {
	cbs_rebuild_self(argv);
	cbs_shift_args(&argc, &argv);

	Cbs_Proc_Infos procs = {0};

	char *current_arg;
	while ((current_arg = cbs_shift_args(&argc, &argv))) {
		if (cbs_str_eq(current_arg, "build")) {
			cbs_run("mkdir", "-p", "build");
			Cbs_File_Paths file_paths = {0};
			cbs_file_paths_build_file_ext(&file_paths, "./", ".c");
			for (int i = 0; i < file_paths.count; ++i) {
				printf("\t%s\n", cbs_strip_file_ext(file_paths.items[i]));
			}
			cbs_file_paths_clear(&file_paths);
			cbs_file_paths_build_file_ext(&file_paths, "./test", ".c");
			for (int i = 0; i < file_paths.count; ++i) {
				printf("\t%s\n", cbs_strip_file_ext(file_paths.items[i]));
			}
			cbs_file_paths_clear(&file_paths);
			cbs_file_paths_append(&file_paths, "hello.c");
			if (cbs_needs_rebuild_file_paths("./build/hello", file_paths)) {
				cbs_proc_infos_append(&procs, cbs_async_run("cc", CFLAGS, "-o", "./build/hello", "hello.c"));
			}
			cbs_file_paths_clear(&file_paths);
			cbs_proc_infos_build(&procs, \
				cbs_async_run("mkdir", "-p", "test1"), \
				cbs_async_run("mkdir", "-p", "test2"), \
				cbs_async_run("mkdir", "-p", "test3"), \
				cbs_async_run("mkdir", "-p", "test4") \
			);
			cbs_async_wait(&procs);
		} else if(cbs_str_eq(current_arg, "run")) {
			if (!cbs_try_run("./build/hello")) {
				cbs_run("./cbs", "build", "run");
			}
		} else if(cbs_str_eq(current_arg, "clean")) {
			cbs_run("rm", "-rf", "build");
			cbs_proc_infos_build(&procs, \
				cbs_async_run("rmdir", "test1"), \
				cbs_async_run("rmdir", "test2"), \
				cbs_async_run("rmdir", "test3"), \
				cbs_async_run("rmdir", "test4") \
			);
			cbs_async_wait(&procs);
		} else {
			cbs_error("Not a valid argument");
		}
	}

	return 0;
}
