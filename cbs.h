#ifndef _CBS_H_
#define _CBS_H_

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define cbs_da_struct(Type, Name) \
	typedef struct { \
		Type *items; \
		int count; \
		int capacity; \
	} Name
cbs_da_struct(char *, Cbs_Cmd);
typedef struct {
	Cbs_Cmd cmd;
	FILE *output;
	pid_t pid;
} Cbs_Proc_Info;
cbs_da_struct(Cbs_Proc_Info, Cbs_Proc_Infos);
cbs_da_struct(char *, Cbs_File_Names);

#define cbs_log(x) printf("%s\n", x)
#define cbs_error(x) exit((fprintf(stderr, "ERROR: %s (%s:%u)\n", x, __FILE__, __LINE__), (errno ? (perror("INFO"), errno) : 1)));
#define cbs_malloc_error cbs_error("Process ran out of memory")

#define cbs_append_item(da, item) \
	do { \
		if ((da)->capacity == 0) { \
			(da)->capacity = 1; \
			if (((da)->items = malloc(sizeof(*(da)->items))) == NULL) cbs_malloc_error; \
		} else if ((da)->count >= (da)->capacity) { \
			(da)->capacity *= 2; \
			if (((da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items))) == NULL) cbs_malloc_error; \
		} \
		(da)->items[(da)->count++] = item; \
	} while(0)
#define cbs_append_list(da, list, size) \
	do { \
		if ((da)->capacity == 0) { \
			(da)->capacity = 2 * size; \
			if (((da)->items = malloc((da)->capacity * sizeof(*(da)->items))) == NULL) cbs_malloc_error; \
		} else if ((da)->count + size - 1 >= (da)->capacity) { \
			while ((da)->count + size - 1 >= (da)->capacity) (da)->capacity *= 2; \
			if (((da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items))) == NULL) cbs_malloc_error; \
		} \
		memcpy(&(da)->items[(da)->count], list, size * sizeof(*(da)->items)); \
		(da)->count += size; \
	} while(0)
#define cbs_append_items(Type, da, ...) cbs_append_list(da, ((Type[]) {__VA_ARGS__}), (sizeof((Type[]) {__VA_ARGS__}) / sizeof(Type)))
#define cbs_clear(da) \
	do { \
		if ((da)->items) free((da)->items); \
		(da)->items = NULL; \
		(da)->count = (da)->capacity = 0; \
	} while(0)

void cbs_rebuild_self(char **argv);

char *cbs_shift_args(int *argc_p, char ***argv_p);
#define cbs_str_eq(str1, str2) strcmp(str1, str2) == 0
bool cbs_file_exists(char *file_name);
#define cbs_files_exist(file_name, ...) cbs_files_exist_nt(file_name, __VA_ARGS__, NULL)
#define cbs_needs_rebuild(target, ...) cbs_needs_rebuild_nt(target, __VA_ARGS__, NULL)
// TODO: Things above take full file name, things below take only file name (not extension). Bridge this gap

#define cbs_file_names_append(file_names, file_name) cbs_append_item(file_names, file_name)
#define cbs_file_names_build(file_names, file_name) cbs_append_items(char *, file_names, __VA_ARGS__)
void cbs_file_names_build_with_ext(Cbs_File_Names *file_names, char *dir_name, char *ext);
#define cbs_file_names_clear(file_names) cbs_clear(file_names)

#define cbs_cmd_append(cmd, string) cbs_append_item(cmd, string)
#define cbs_cmd_build(cmd, ...) cbs_append_items(char *, cmd, __VA_ARGS__)
#define cbs_cmd_print(cmd) \
	do { \
		printf("  "); \
		for (int i = 0; i < cmd.count - 1; ++i) printf("%s ", cmd.items[i]); \
		printf("%s\n", cmd.items[cmd.count - 1]); \
	} while(0)
#define cbs_cmd_clear(cmd) cbs_clear(cmd)
bool cbs_cmd_try_run(Cbs_Cmd *cmd);
void cbs_cmd_run(Cbs_Cmd *cmd);

#define cbs_try_run(...) cbs_try_run_nt(__VA_ARGS__, NULL)
#define cbs_run(...) \
	do { \
		Cbs_Cmd cmd = {0}; \
		cbs_cmd_build(&cmd, __VA_ARGS__); \
		cbs_cmd_run(&cmd); \
	} while(0)

Cbs_Proc_Info cbs_cmd_async_run(Cbs_Cmd *cmd);
#define cbs_async_run(...) cbs_async_run_nt(__VA_ARGS__, NULL)
#define cbs_proc_infos_append(procs, proc) cbs_append_item(procs, proc)
#define cbs_proc_infos_build(procs, ...) cbs_append_items(Cbs_Proc_Info, procs, __VA_ARGS__)
void cbs_async_wait(Cbs_Proc_Infos *procs);

#endif // _CBS_H_

#ifdef CBS_IMPLEMENTATION

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char *cbs_shift_args(int *argc_p, char ***argv_p) {
	if ((*argc_p)-- == 0) return NULL;
	return *((*argv_p)++);
}

bool cbs_file_exists(char *file_name) {
	struct stat temp;
	if (stat(file_name, &temp) == -1) {
		if (errno == ENOENT) return false;
		cbs_error("Unable to access file");
	}
	return true;
}

bool cbs_files_exist_nt(char *file_name, ...) {
	if (!cbs_file_exists(file_name)) return false;
	va_list args;
	va_start(args, file_name);
	char *arg;
	while ((arg = va_arg(args, char *))) {
		if (!cbs_file_exists(arg)) {
			va_end(args);
			return false;
		}
	}
	va_end(args);
	return true;
}

bool cbs_needs_rebuild_nt(char *target, ...) {
	struct stat temp;
	if (!cbs_file_exists(target)) return true;
	stat(target, &temp);
	__darwin_time_t target_mtime = temp.st_mtime;

	va_list args;
	va_start(args, target);
	char *dep = va_arg(args, char *);
	while (dep) {
		if (!cbs_file_exists(dep)) cbs_error("Could not open dependency when checking target rebuild");
		stat(dep, &temp);
		__darwin_time_t dep_mtime = temp.st_mtime;
		if (target_mtime < dep_mtime) {
			va_end(args);
			return true;
		}
		dep = va_arg(args, char *);
	}
	va_end(args);
	return false;
}

static bool cbs_file_has_ext(char *file_name, char *ext) {
	if (file_name == NULL || ext == NULL) return false;
	char *file_name_p = file_name, *ext_p = ext;
	while (*file_name_p != '.' && *file_name_p != '\0') ++file_name_p;
	if (*file_name_p++ == '\0') return false;
	while (*file_name_p != '\0' && *ext_p != '\0') if (*file_name_p++ != *ext_p++) return false;
	if (*file_name_p != '\0' || *ext_p != '\0') return false;
	return true;
}

static char *cbs_file_strip_ext(char *file_name) {
	char *char_p = file_name;
	while (*char_p++ != '.');
	int file_name_len = char_p - file_name - 1;
	char *result;
	if ((result = malloc((file_name_len + 1) * sizeof(char))) == NULL) cbs_malloc_error;
	strncpy(result, file_name, file_name_len);
	return result;
}

void cbs_file_names_build_with_ext(Cbs_File_Names *file_names, char *dir_name, char *ext) {
	DIR *dir;
	if ((dir = opendir(dir_name)) == NULL) cbs_error("Unable to open directory for search");
	struct dirent *entry = readdir(dir);
	while (entry) {
		char *file_name = entry->d_name;
		if (entry->d_type == DT_REG && cbs_file_has_ext(file_name, ext)) {
			cbs_append_item(file_names, cbs_file_strip_ext(file_name));
		}
		entry = readdir(dir);
	}
	if (closedir(dir) == -1) cbs_error("Unable to close directory after search");
}

void cbs_cmd_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info proc = cbs_cmd_async_run(cmd);
	Cbs_Proc_Infos procs = {0};
	cbs_proc_infos_append(&procs, proc);
	cbs_async_wait(&procs);
}

static void cbs_file_copy(FILE *dst, FILE *src) {
	if (fseek(src, 0, SEEK_END) == -1) cbs_error("Unable to seek file during copy");
	long src_size;
	if ((src_size = ftell(src)) == -1) cbs_error("Unable to access file during copy");
	if (src_size == 0) return;

	char *buffer;
	if ((buffer = malloc(src_size * sizeof(char))) == NULL) cbs_malloc_error;
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

bool cbs_cmd_try_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info proc = cbs_cmd_async_run(cmd);
	int status = 0;
	waitpid(proc.pid, &status, 0);
	cbs_cmd_print(proc.cmd);
	cbs_file_copy(stdout, proc.output);
	if (status) cbs_log("Previous command ran unsuccessfully, continuing build");
	return !status;
}

#define cbs_append_from_va(da, Type, first) \
	do { \
		cbs_append_item(&da, first); \
		va_list args; \
		va_start(args, first); \
		char *arg = va_arg(args, Type); \
		while (arg) { \
			cbs_append_item(&da, arg); \
			arg = va_arg(args, Type); \
		} \
		va_end(args); \
	} while(0)

static bool cbs_try_run_nt(char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs_append_from_va(cmd, char *, string);
	return cbs_cmd_try_run(&cmd);
}

void cbs_rebuild_self(char **argv) {
	char *this_file_name = argv[0];

	if (!cbs_needs_rebuild(this_file_name, "cbs.c", "cbs.h")) return;

	FILE *this_file, *backup_file;
	if ((this_file = fopen(this_file_name, "r+")) == NULL) cbs_error("Unable to open file for rebuilding, bootstrapping may be necessary");
	if ((backup_file = tmpfile()) == NULL) cbs_error("Unable to open backup file for rebuilding, bootstraping may be necessary");

	cbs_file_copy(backup_file, this_file);
	cbs_log("Rebuilding cbs");
	if (!cbs_try_run("cc", "-o", this_file_name, "cbs.c")) {
		cbs_log("Rebuild unsuccessful, undoing backup");
		cbs_file_copy(this_file, backup_file);
		fclose(this_file);
		cbs_error("Unable to rebuild cbs, bootstrapping may be necessary");
	}
	cbs_log("Rebuild successful");
	fclose(this_file);
	if (execvp(this_file_name, argv) == -1) cbs_error("Rebuilt build command ran unsuccessfully, bootstrapping may be necessary");
}

Cbs_Proc_Info cbs_cmd_async_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info result = {0};
	result.cmd = *cmd;
	if ((result.cmd.items = malloc(cmd->capacity * sizeof(char *))) == NULL) cbs_malloc_error;
	memcpy(result.cmd.items, cmd->items, cmd->count * sizeof(char *));
	result.output = tmpfile();

	cbs_cmd_append(cmd, NULL);

	if ((result.pid = fork()) == 0) {
		dup2(fileno(result.output), STDOUT_FILENO);
		dup2(fileno(result.output), STDERR_FILENO);
		if (execvp(cmd->items[0], cmd->items) == -1) cbs_error("Unable to run command, check command syntax");
	}

	cbs_cmd_clear(cmd);
	return result;
}

static Cbs_Proc_Info cbs_async_run_nt(char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs_append_from_va(cmd, char *, string);
	return cbs_cmd_async_run(&cmd);
}

void cbs_async_wait(Cbs_Proc_Infos *procs) {
	if (procs == NULL || procs->count == 0) return;
	int status = 0;
	for (int i = 0; i < procs->count; ++i) {
		Cbs_Proc_Info proc = procs->items[i];
		waitpid(proc.pid, &status, 0);
		cbs_cmd_print(proc.cmd);
		cbs_file_copy(stdout, proc.output);
		if (status) cbs_error("Previous command ran unsuccessfully, stopping build");
	}
	cbs_clear(procs);
}

#endif // CBS_IMPLEMENTATION
