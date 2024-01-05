#ifndef _CBS_H_
#define _CBS_H_

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define cbs_log(x) printf("%s\n", x)
#define cbs_error(x) exit((fprintf(stderr, "ERROR: %s (%s:%u)\n", x, __FILE__, __LINE__), (errno ? (perror("INFO"), errno) : 1)));

#define cbs__malloc_error cbs_error("Process ran out of memory")
#define cbs__append_item(da, item) \
	do { \
		if ((da)->capacity == 0) { \
			(da)->capacity = 1; \
			if (((da)->items = malloc(sizeof(*(da)->items))) == NULL) cbs__malloc_error; \
		} else if ((da)->count >= (da)->capacity) { \
			(da)->capacity *= 2; \
			if (((da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items))) == NULL) cbs__malloc_error; \
		} \
		(da)->items[(da)->count++] = item; \
	} while(0)
#define cbs__append_list(da, list, size) \
	do { \
		if ((da)->capacity == 0) { \
			(da)->capacity = 2 * size; \
			if (((da)->items = malloc((da)->capacity * sizeof(*(da)->items))) == NULL) cbs__malloc_error; \
		} else if ((da)->count + size - 1 >= (da)->capacity) { \
			while ((da)->count + size - 1 >= (da)->capacity) (da)->capacity *= 2; \
			if (((da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items))) == NULL) cbs__malloc_error; \
		} \
		memcpy(&(da)->items[(da)->count], list, size * sizeof(*(da)->items)); \
		(da)->count += size; \
	} while(0)
#define cbs__append_items(Type, da, ...) cbs__append_list(da, ((Type[]) {__VA_ARGS__}), (size_t) (sizeof((Type[]) {__VA_ARGS__}) / sizeof(Type)))
#define cbs__clear(da) \
	do { \
		if ((da)->items) free((da)->items); \
		(da)->items = NULL; \
		(da)->count = (da)->capacity = 0; \
	} while(0)
#define cbs__da_struct(Type, Name) \
	typedef struct { \
		Type *items; \
		size_t count; \
		size_t capacity; \
	} Name

void cbs_rebuild_self(char *const *argv);

char *cbs_shift_args(int *argc_p, char ***argv_p);
#define cbs_str_eq(str1, str2) strcmp(str1, str2) == 0
#define cbs_str_cat(string, ...) cbs__str_cat_nt(string, __VA_ARGS__, NULL)
bool cbs_file_exists(const char *file_path);
#define cbs_files_exist(file_path, ...) cbs__files_exist_nt(file_path, __VA_ARGS__, NULL)
char *cbs_get_file_name(const char *file_path);
char *cbs_strip_file_ext(const char *file_path);

cbs__da_struct(const char *, Cbs_File_Paths);
#define cbs_file_paths_append(file_paths, file_path) cbs__append_item(file_paths, file_path)
#define cbs_file_paths_build(file_paths, ...) cbs__append_items(const char *, file_paths, __VA_ARGS__)
void cbs_file_paths_build_file_ext(Cbs_File_Paths *file_paths, const char *dir_path, const char *ext);
#define cbs_file_paths_clear(file_paths) cbs__clear(file_paths)

#define cbs_needs_rebuild(target, ...) cbs__needs_rebuild_nt(target, __VA_ARGS__, NULL)
bool cbs_needs_rebuild_file_paths(const char *target, Cbs_File_Paths deps);

cbs__da_struct(const char *, Cbs_Cmd);
#define cbs_cmd_append(cmd, string) cbs__append_item(cmd, string)
#define cbs_cmd_build(cmd, ...) cbs__append_items(const char *, cmd, __VA_ARGS__)
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

#define cbs_try_run(...) cbs__try_run_nt(__VA_ARGS__, NULL)
#define cbs_run(...) \
	do { \
		Cbs_Cmd cmd = {0}; \
		cbs_cmd_build(&cmd, __VA_ARGS__); \
		cbs_cmd_run(&cmd); \
	} while(0)

typedef struct {
	Cbs_Cmd cmd;
	FILE *output;
	pid_t pid;
} Cbs_Async_Proc;
cbs__da_struct(Cbs_Async_Proc, Cbs_Async_Procs);
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

static char *cbs__str_cat_nt(const char *string, ...) {
	size_t sum_len = strlen(string);

	va_list args;
	va_start(args, string);
	const char *arg;
	while ((arg = va_arg(args, const char *))) sum_len += strlen(arg);
	va_end(args);

	char *result = malloc((sum_len + 1) * sizeof(char));
	char *result_p = result;
	result_p = stpncpy(result_p, string, strlen(string));

	va_start(args, string);
	while ((arg = va_arg(args, const char *))) result_p = stpncpy(result_p, arg, strlen(arg));
	va_end(args);
	*result_p = '\0';

	return result;
}

bool cbs_file_exists(const char *file_path) {
	struct stat temp;
	if (stat(file_path, &temp) == -1) {
		if (errno == ENOENT) return false;
		cbs_error("Unable to access file");
	}
	return true;
}

static bool cbs__files_exist_nt(const char *file_path, ...) {
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

char *cbs_get_file_name(const char *file_path) {
	size_t file_len = strlen(file_path);
	if (file_len == 0) return "";
	while (file_path[--file_len] != '/') if (file_len == 0) break;
	const char *file_name = file_len == 0 && *file_path != '/' ? file_path : &file_path[file_len + 1];
	char *result;
	if ((result = strdup(file_name)) == NULL) cbs__malloc_error;
	return result;
}

char *cbs_strip_file_ext(const char *file_path) {
	size_t file_len = strlen(file_path);
	if (file_len == 0) return "";
	while (file_path[--file_len] != '.') if (file_len == 0) return "";
	char *result;
	if ((result = strndup(file_path, file_len)) == NULL) cbs__malloc_error;
	return result;
}

static bool cbs__file_has_ext(const char *file_path, const char *ext) {
	size_t file_len = strlen(file_path), ext_len = strlen(ext);
	if (file_len <= ext_len) return false;
	for (--file_len, --ext_len; ext_len > 0; --file_len, --ext_len)
		if (file_path[file_len] != ext[ext_len]) return false;
	return file_path[file_len] == ext[ext_len];
}

void cbs_file_paths_build_file_ext(Cbs_File_Paths *file_paths, const char *dir_path, const char *ext) {
	DIR *dir;
	if ((dir = opendir(dir_path)) == NULL) cbs_error("Unable to open directory for search");
	struct dirent *entry = readdir(dir);
	while (entry) {
		const char *file_path = entry->d_name;
		if (entry->d_type == DT_REG && cbs__file_has_ext(file_path, ext)) {
			char *result_path;
			if ((result_path = malloc((strlen(dir_path) + strlen(file_path) + 2) * sizeof(char))) == NULL) cbs__malloc_error;
			strncpy(result_path, dir_path, strlen(dir_path) + 1);
			if (dir_path[strlen(dir_path) - 1] != '/') strcat(result_path, "/");
			strcat(result_path, file_path);
			cbs_file_paths_append(file_paths, result_path);
		}
		entry = readdir(dir);
	}
	if (closedir(dir) == -1) cbs_error("Unable to close directory after search");
}

static bool cbs__needs_rebuild_nt(const char *target, ...) {
	struct stat temp;
	if (!cbs_file_exists(target)) return true;
	stat(target, &temp);
	size_t target_mtime = (size_t) temp.st_mtime;

	va_list args;
	va_start(args, target);
	const char *dep;
	while ((dep = va_arg(args, const char *))) {
		if (!cbs_file_exists(dep)) cbs_error("Could not open dependency when checking target rebuild");
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
		if (!cbs_file_exists(dep)) cbs_error("Could not open dependency when checking target rebuild");
		stat(dep, &temp);
		size_t dep_mtime = (size_t) temp.st_mtime;
		if (target_mtime < dep_mtime) return true;
	}
	return false;
}

void cbs_cmd_build_file_paths(Cbs_Cmd *cmd, Cbs_File_Paths file_paths) {
	for (size_t i = 0; i < file_paths.count; ++i) cbs_cmd_append(cmd, file_paths.items[i]);
}

#define cbs__cmd_run_save_status(cmd) \
	do { \
		cbs_cmd_print(*cmd); \
		cbs_cmd_append(cmd, NULL); \
		pid_t child_pid; \
		if ((child_pid = fork()) == 0) \
			if (execvp(cmd->items[0], (char *const *) cmd->items) == -1) cbs_error("Unable to run command, check command syntax"); \
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

static bool cbs__try_run_nt(const char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs__append_from_va(cmd, const char *, string);
	return cbs_cmd_try_run(&cmd);
}

static void cbs__file_copy(FILE *dst, FILE *src) {
	if (fseek(src, 0, SEEK_END) == -1) cbs_error("Unable to seek file during copy");
	long src_size;
	if ((src_size = ftell(src)) == -1) cbs_error("Unable to access file during copy");
	if (src_size == 0) return;

	char *buffer;
	if ((buffer = malloc(src_size * sizeof(char))) == NULL) cbs__malloc_error;
	if (fseek(src, 0, SEEK_SET) == -1) cbs_error("Unable to seek file during copy");
	size_t file_index = src_size, n;
	char *buffer_p = buffer;
	while(file_index > 0) {
		n = fread(buffer_p, 1, file_index, src);
		if (ferror(src)) {
			if (src != stderr) fclose(src); 
			if (dst != stderr) fclose(dst);
			cbs_error("Unable to read from source file while copying");
		}
		file_index -= n;
		buffer_p += n;
	}
	file_index = src_size;
	buffer_p = buffer;
	while (file_index > 0) {
		n = fwrite(buffer_p, 1, file_index, dst);
		if (ferror(dst)) {
			if (src != stderr) fclose(src); 
			if (dst != stderr) fclose(dst);
			cbs_error("Unable to write to destination file while copying");
		}
		file_index -= n;
		buffer_p += n;
	}
	free(buffer);
}

void cbs_rebuild_self(char *const *argv) {
	const char *this_file_name = argv[0];

	if (!cbs_needs_rebuild(this_file_name, "cbs.c", "cbs.h")) return;

	FILE *this_file, *backup_file;
	if ((this_file = fopen(this_file_name, "r+")) == NULL) cbs_error("Unable to open file for rebuilding, bootstrapping may be necessary");
	if ((backup_file = tmpfile()) == NULL) cbs_error("Unable to open backup file for rebuilding, bootstraping may be necessary");

	cbs__file_copy(backup_file, this_file);
	cbs_log("Rebuilding cbs");
	if (!cbs_try_run("cc", "-o", this_file_name, "cbs.c")) {
		cbs_log("Rebuild unsuccessful, undoing backup");
		cbs__file_copy(this_file, backup_file);
		fclose(this_file);
		cbs_error("Unable to rebuild cbs, bootstrapping may be necessary");
	}
	cbs_log("Rebuild successful");
	fclose(this_file);
	if (execvp(this_file_name, argv) == -1) cbs_error("Rebuilt build command ran unsuccessfully, bootstrapping may be necessary");
}

void cbs_cmd_async_run(Cbs_Async_Procs *procs, Cbs_Cmd *cmd) {
	Cbs_Async_Proc proc = {0};
	proc.cmd = *cmd;
	if ((proc.cmd.items = malloc(cmd->capacity * sizeof(char *))) == NULL) cbs__malloc_error;
	memcpy(proc.cmd.items, cmd->items, cmd->count * sizeof(char *));
	proc.output = tmpfile();

	cbs_cmd_append(cmd, NULL);

	if ((proc.pid = fork()) == 0) {
		dup2(fileno(proc.output), STDOUT_FILENO);
		dup2(fileno(proc.output), STDERR_FILENO);
		if (execvp(cmd->items[0], (char *const *) cmd->items) == -1) cbs_error("Unable to run command, check command syntax");
	}

	cbs_cmd_clear(cmd);
	cbs__append_item(procs, proc);
}

void cbs_async_wait(Cbs_Async_Procs *procs) {
	if (procs == NULL || procs->count == 0) return;
	int status = 0;
	for (size_t i = 0; i < procs->count; ++i) {
		Cbs_Async_Proc proc = procs->items[i];
		waitpid(proc.pid, &status, 0);
		cbs_cmd_print(proc.cmd);
		cbs__file_copy(stdout, proc.output);
		if (status) cbs_error("Previous command ran unsuccessfully, stopping build");
	}
	cbs__clear(procs);
}

#endif // CBS_IMPLEMENTATION
