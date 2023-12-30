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
			Cbs_File_Names file_names = {0};
			cbs_file_names_build_with_ext(&file_names, "./", "c"); // TODO: Have extensions include periods
			for (int i = 0; i < file_names.count; ++i) {
				printf("\t%s" ".c" "\n", file_names.items[i]);
			}	
			cbs_file_names_clear(&file_names);
			cbs_file_names_build_with_ext(&file_names, "./test", "c");
			for (int i = 0; i < file_names.count; ++i) {
				printf("\t%s" ".c" "\n", file_names.items[i]);
			}	
			cbs_file_names_clear(&file_names);
			if (cbs_needs_rebuild("./build/hello", "hello.c")) {
				cbs_proc_infos_append(&procs, cbs_async_run("cc", CFLAGS, "-o", "./build/hello", "hello.c"));
			}
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
