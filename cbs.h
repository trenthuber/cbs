#ifndef _CBS_H_
#define _CBS_H_

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// cbs logging
#define cbs_log(x) printf("%s\n", x)
#define cbs_error(x) \
	do { \
		fprintf(stderr, "ERROR: %s (%s:%u)\n", x, __FILE__, __LINE__); \
		exit(errno ? (perror("INFO"), errno) : EXIT_FAILURE); \
	} while(0)
#define cbs__malloc_error cbs_error("Process ran out of memory")

// Useful macros for generic dynamic arrays used in cbs data structures
#define cbs__da_struct(Type, Name) \
	typedef struct { \
		Type *items; \
		size_t count; \
		size_t cap; \
	} Name
#define cbs__append_item(da, item) \
	do { \
		if ((da)->cap == 0) { \
			(da)->cap = 1; \
			if (((da)->items = malloc(sizeof(*(da)->items))) == NULL) \
				cbs__malloc_error; \
		} else if ((da)->count >= (da)->cap) { \
			(da)->cap *= 2; \
			(da)->items = realloc((da)->items, (da)->cap * sizeof(*(da)->items)); \
			if ((da)->items == NULL) \
				cbs__malloc_error; \
		} \
		(da)->items[(da)->count++] = item; \
	} while(0)
#define cbs__append_list(da, list, size) \
	do { \
		if ((da)->cap == 0) { \
			(da)->cap = 2 * size; \
			if (((da)->items = malloc((da)->cap * sizeof(*(da)->items))) == NULL) \
				cbs__malloc_error; \
		} else if ((da)->count + size - 1 >= (da)->cap) { \
			while ((da)->count + size - 1 >= (da)->cap) (da)->cap *= 2; \
			(da)->items = realloc((da)->items, (da)->cap * sizeof(*(da)->items)); \
			if ((da)->items == NULL) \
				cbs__malloc_error; \
		} \
		memcpy(&(da)->items[(da)->count], list, size * sizeof(*(da)->items)); \
		(da)->count += size; \
	} while(0)
#define cbs__append_items(Type, da, ...) \
	cbs__append_list(da, ((Type[]) {__VA_ARGS__}), \
	                 (size_t) (sizeof((Type[]) {__VA_ARGS__}) / sizeof(Type)))
#define cbs__clear(da) \
	do { \
		if ((da)->items) free((da)->items); \
		(da)->items = NULL; \
		(da)->count = (da)->cap = 0; \
	} while(0)

void cbs_rebuild_self(char *const *argv);

// Utility functions: parsing command line options and manipulating file paths
char *cbs_shift_args(int *argc_p, char ***argv_p);
#define cbs_string_eq(str1, str2) strcmp(str1, str2) == 0
#define cbs_string_build(string, ...) \
	cbs__string_build_nt(string, __VA_ARGS__, NULL)
char *cbs_get_file_name(const char *file_path);
char *cbs_strip_file_ext(const char *file_path);

// File paths
cbs__da_struct(const char *, Cbs_File_Paths);
#define cbs_file_paths_append(file_paths, file_path) \
	cbs__append_item(file_paths, file_path)
#define cbs_file_paths_build(file_paths, ...) \
	cbs__append_items(const char *, file_paths, __VA_ARGS__)
void cbs_file_paths_build_file_ext(Cbs_File_Paths *file_paths, 
                                   const char *dir_path, 
                                   const char *ext);
#define cbs_file_paths_for_each(file_path, file_paths) \
	for (const char *file_path = file_paths.items[0], \
	     **current = &file_paths.items[0], \
	     **end = &file_paths.items[file_paths.count - 1]; \
	     current <= end; \
	     ++current, file_path = *current)
#define cbs_file_paths_clear(file_paths) cbs__clear(file_paths)
#define cbs_file_paths_free(...) cbs__file_paths_free_nt(__VA_ARGS__, NULL)

// Useful conditionals, i.e., file caching
bool cbs_file_exists(const char *file_path);
#define cbs_files_exist(file_path, ...) \
	cbs__files_exist_nt(file_path, __VA_ARGS__, NULL)
#define cbs_needs_rebuild(target, ...) \
	cbs__needs_rebuild_nt(target, __VA_ARGS__, NULL)
bool cbs_needs_rebuild_file_paths(const char *target, Cbs_File_Paths deps);

// Build and run commands
cbs__da_struct(const char *, Cbs_Cmd);
#define cbs_cmd_append(cmd, string) cbs__append_item(cmd, string)
#define cbs_cmd_build(cmd, ...) \
	cbs__append_items(const char *, cmd, __VA_ARGS__)
void cbs_cmd_build_file_paths(Cbs_Cmd *cmd, Cbs_File_Paths file_paths);
#define cbs_cmd_print(cmd) \
	do { \
		printf("  "); \
		for (size_t i = 0; i < (cmd).count - 1; ++i) printf("%s ", (cmd).items[i]); \
		printf("%s\n", (cmd).items[(cmd).count - 1]); \
	} while(0)
#define cbs_cmd_clear(cmd) cbs__clear(cmd)
bool cbs_cmd_try_run(Cbs_Cmd *cmd);
void cbs_cmd_run(Cbs_Cmd *cmd);

// Run commands directly
#define cbs_try_run(...) cbs__try_run_nt(__VA_ARGS__, NULL)
#define cbs_run(...) \
	do { \
		Cbs_Cmd cmd = {0}; \
		cbs_cmd_build(&cmd, __VA_ARGS__); \
		cbs_cmd_run(&cmd); \
	} while(0)

// Run commands asynchronously
typedef struct {
	Cbs_Cmd cmd;
	FILE *output;
	pid_t pid;
} Cbs__Async_Proc;
cbs__da_struct(Cbs__Async_Proc, Cbs_Async_Procs);
void cbs_cmd_async_run(Cbs_Async_Procs *procs, Cbs_Cmd *cmd);
#define cbs_async_run(procs, ...) \
	do { \
		Cbs_Cmd cmd = {0}; \
		cbs_cmd_build(&cmd, __VA_ARGS__); \
		cbs_cmd_async_run(procs, &cmd); \
	} while(0)
void cbs_async_wait(Cbs_Async_Procs *procs);

#endif // _CBS_H_

#ifdef CBS_IMPLEMENTATION

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>

char *cbs_shift_args(int *argc_p, char ***argv_p) {
	if ((*argc_p)-- == 0) return NULL;
	return *((*argv_p)++);
}

char *cbs__string_build_nt(const char *string, ...) {
	size_t sum_len = strlen(string);

	va_list args;
	va_start(args, string);
	const char *arg;
	while ((arg = va_arg(args, const char *))) sum_len += strlen(arg);
	va_end(args);

	char *result = malloc((sum_len + 1) * sizeof(char));
	if (result == NULL) cbs__malloc_error;
	char *result_p = result;
	result_p = stpncpy(result_p, string, strlen(string));

	va_start(args, string);
	while ((arg = va_arg(args, const char *)))
		result_p = stpncpy(result_p, arg, strlen(arg));
	va_end(args);
	*result_p = '\0';

	return result;
}

char *cbs_get_file_name(const char *file_path) {
	size_t file_len = strlen(file_path);
	if (file_len == 0) return "";
	while (file_path[--file_len] != '/') if (file_len == 0) break;
	const char *file_name = file_len == 0 && *file_path != '/'
	                      ? file_path
	                      : &file_path[file_len + 1];
	char *result = strdup(file_name);
	if (result == NULL) cbs__malloc_error;
	return result;
}

char *cbs_strip_file_ext(const char *file_path) {
	size_t file_len = strlen(file_path);
	if (file_len == 0) return "";
	while (file_path[--file_len] != '.') if (file_len == 0) return "";
	char *result = strndup(file_path, file_len);
	if (result == NULL) cbs__malloc_error;
	return result;
}

static bool cbs__file_has_ext(const char *file_path, const char *ext) {
	size_t file_len = strlen(file_path), ext_len = strlen(ext);
	if (file_len <= ext_len) return false;
	for (--file_len, --ext_len; ext_len > 0; --file_len, --ext_len)
		if (file_path[file_len] != ext[ext_len]) return false;
	return file_path[file_len] == ext[ext_len];
}

void cbs_file_paths_build_file_ext(Cbs_File_Paths *file_paths,
                                   const char *dir_path,
                                   const char *ext) {
	DIR *dir = opendir(dir_path);
	if (dir == NULL) cbs_error("Unable to open directory for search");
	struct dirent *entry = readdir(dir);
	while (entry) {
		const char *file_path = entry->d_name;
		if (entry->d_type == DT_REG && cbs__file_has_ext(file_path, ext)) {
			char *result_path = malloc((strlen(dir_path) + strlen(file_path) + 2) *
			                           sizeof(char));
			if (result_path == NULL) cbs__malloc_error;
			strncpy(result_path, dir_path, strlen(dir_path) + 1);
			if (dir_path[strlen(dir_path) - 1] != '/') strcat(result_path, "/");
			strcat(result_path, file_path);
			cbs_file_paths_append(file_paths, result_path);
		}
		entry = readdir(dir);
	}
	if (closedir(dir) == -1) cbs_error("Unable to close directory after search");
}

void cbs__file_paths_free_nt(Cbs_File_Paths *file_paths, ...) {
	for (size_t i = 0; i < file_paths->count; ++i)
		free((char *) file_paths->items[i]);
	cbs_file_paths_clear(file_paths);

	va_list args;
	va_start(args, file_paths);
	Cbs_File_Paths *arg;
	while ((arg = va_arg(args, Cbs_File_Paths *))) {
		for (size_t i = 0; i < arg->count; ++i) free((char *) arg->items[i]);
		cbs_file_paths_clear(arg);
	}
	va_end(args);
}

bool cbs_file_exists(const char *file_path) {
	struct stat temp;
	if (stat(file_path, &temp) == -1) {
		if (errno == ENOENT) return false;
		cbs_error("Unable to access file");
	}
	return true;
}

bool cbs__files_exist_nt(const char *file_path, ...) {
	if (!cbs_file_exists(file_path)) return false;
	va_list args;
	va_start(args, file_path);
	const char *arg;
	while ((arg = va_arg(args, const char *)))
		if (!cbs_file_exists(arg)) {
			va_end(args);
			return false;
		}
	va_end(args);
	return true;
}

bool cbs__needs_rebuild_nt(const char *target, ...) {
	struct stat temp;
	if (!cbs_file_exists(target)) return true;
	stat(target, &temp);
	size_t target_mtime = (size_t) temp.st_mtime;

	va_list args;
	va_start(args, target);
	const char *dep;
	while ((dep = va_arg(args, const char *))) {
		if (!cbs_file_exists(dep))
			cbs_error("Could not open dependency when checking target rebuild");
		stat(dep, &temp);
		size_t dep_mtime = (size_t) temp.st_mtime;
		if (target_mtime < dep_mtime) {
			va_end(args);
			return true;
		}
	}
	va_end(args);
	return false;
}

bool cbs_needs_rebuild_file_paths(const char *target, Cbs_File_Paths deps) {
	struct stat temp;
	if (!cbs_file_exists(target)) return true;
	stat(target, &temp);
	size_t target_mtime = (size_t) temp.st_mtime;

	for (size_t i = 0; i < deps.count; ++i) {
		const char *dep = deps.items[i];
		if (!cbs_file_exists(dep))
			cbs_error("Could not open dependency when checking target rebuild");
		stat(dep, &temp);
		size_t dep_mtime = (size_t) temp.st_mtime;
		if (target_mtime < dep_mtime) return true;
	}
	return false;
}

void cbs_cmd_build_file_paths(Cbs_Cmd *cmd, Cbs_File_Paths file_paths) {
	cbs_file_paths_for_each(file_path, file_paths) cbs_cmd_append(cmd, file_path);
}

#define cbs__cmd_run_save_status(cmd) \
	do { \
		cbs_cmd_print(*cmd); \
		cbs_cmd_append(cmd, NULL); \
		pid_t child_pid; \
		if ((child_pid = fork()) == 0 \
		    && (execvp(cmd->items[0], (char *const *) cmd->items) == -1)) \
			cbs_error("Unable to run command, check command syntax"); \
		waitpid(child_pid, &status, 0); \
		cbs_cmd_clear(cmd); \
	} while(0)

void cbs_cmd_run(Cbs_Cmd *cmd) {
	int status = 0;
	cbs__cmd_run_save_status(cmd);
	if (status) cbs_error("Previous command ran unsuccessfully, stopping build");
}

bool cbs_cmd_try_run(Cbs_Cmd *cmd) {
	int status = 0;
	cbs__cmd_run_save_status(cmd);
	if (status) cbs_log("Previous command ran unsuccessfully, continuing build");
	return !status;
}

#define cbs__append_from_va(da, Type, first) \
	do { \
		cbs__append_item(&da, first); \
		va_list args; \
		va_start(args, first); \
		Type arg; \
		while ((arg = va_arg(args, Type))) cbs__append_item(&da, arg); \
		va_end(args); \
	} while(0)

bool cbs__try_run_nt(const char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs__append_from_va(cmd, const char *, string);
	return cbs_cmd_try_run(&cmd);
}

void cbs_rebuild_self(char *const *argv) {
	const char *this_file_name = argv[0];
	if (!cbs_needs_rebuild(this_file_name, "cbs.c", "cbs.h")) return;
	cbs_log("Rebuilding cbs");
	if (!cbs_try_run("cc", "-Wall", "-Wextra", "-Wpedantic",
	                 "-o", this_file_name, "cbs.c"))
		cbs_error("Rebuild unsuccessful, bootstrapping may be necessary");
	cbs_log("Rebuild successful");
	if (execvp(this_file_name, argv) == -1)
		cbs_error("Rebuilt build command ran unsuccessfully, "
		          "bootstrapping may be necessary");
}

void cbs_cmd_async_run(Cbs_Async_Procs *procs, Cbs_Cmd *cmd) {
	Cbs__Async_Proc proc = {0};
	proc.cmd = *cmd;
	proc.cmd.items = malloc(cmd->cap * sizeof(char *));
	if (proc.cmd.items == NULL) cbs__malloc_error;
	memcpy(proc.cmd.items, cmd->items, cmd->count * sizeof(char *));
	proc.output = tmpfile();

	cbs_cmd_append(cmd, NULL);

	if ((proc.pid = fork()) == 0) {
		dup2(fileno(proc.output), STDOUT_FILENO);
		dup2(fileno(proc.output), STDERR_FILENO);
		if (execvp(cmd->items[0], (char *const *) cmd->items) == -1)
			cbs_error("Unable to run command, check command syntax");
	}

	cbs_cmd_clear(cmd);
	cbs__append_item(procs, proc);
}

static void cbs__file_print(FILE *file) { 
	if (fseek(file, 0, SEEK_END) == -1)
		cbs_error("Unable to seek file during print");
	long file_size = ftell(file);
	if (file_size == -1) cbs_error("Unable to access file during print");
	if (file_size == 0) return;
	char *buffer = malloc((file_size + 1) * sizeof(char));
	if (buffer == NULL) cbs__malloc_error;
	if (fseek(file, 0, SEEK_SET) == -1)
		cbs_error("Unable to seek file during print");

	size_t file_index = file_size, n;
	char *buffer_p = buffer;
	while(file_index > 0) {
		n = fread(buffer_p, 1, file_index, file);
		if (ferror(file)) cbs_error("Unable to read from file while printing");
		file_index -= n;
		buffer_p += n;
	}
	*buffer_p = '\0';
	printf("%s\n", buffer);
	free(buffer);
}

void cbs_async_wait(Cbs_Async_Procs *procs) {
	if (procs == NULL || procs->count == 0) return;
	int status = 0;
	for (size_t i = 0; i < procs->count; ++i) {
		Cbs__Async_Proc proc = procs->items[i];
		waitpid(proc.pid, &status, 0);
		cbs_cmd_print(proc.cmd);
		cbs__file_print(proc.output);
		if (status) cbs_error("Previous command ran unsuccessfully, stopping build");
	}
	cbs__clear(procs);
}

#endif // CBS_IMPLEMENTATION
