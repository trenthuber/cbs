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
#define cbs_error(x) \
	do { \
		fprintf(stderr, "\nERROR: %s (%s:%u)\n", x, __FILE__, __LINE__); \
		if (errno) perror("INFO"); \
		fprintf(stderr, "\n"); \
		exit(1); \
	} while(0)

#define cbs_append_item(da, item) \
	do { \
		if ((da)->capacity == 0) { \
			(da)->capacity = 1; \
			if (((da)->items = malloc(sizeof(*(da)->items))) == NULL) cbs_error("Process ran out of memory"); \
		} else if ((da)->count >= (da)->capacity) { \
			(da)->capacity *= 2; \
			if (((da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items))) == NULL) cbs_error("Process ran out of memory"); \
		} \
		(da)->items[(da)->count++] = item; \
	} while(0)
#define cbs_append_list(da, list, size) \
	do { \
		if ((da)->capacity == 0) { \
			(da)->capacity = 2 * size; \
			if (((da)->items = malloc((da)->capacity * sizeof(*(da)->items))) == NULL) cbs_error("Process ran out of memory"); \
		} else if ((da)->count + size - 1 >= (da)->capacity) { \
			while ((da)->count + size - 1 >= (da)->capacity) (da)->capacity *= 2; \
			if (((da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items))) == NULL) cbs_error("Process ran out of memory"); \
		} \
		memcpy(&(da)->items[(da)->count], list, size * sizeof(*(da)->items)); \
		(da)->count += size; \
	} while(0)
#define cbs_append_items(Type, da, ...) cbs_append_list(da, ((Type[]) {__VA_ARGS__}), (sizeof((Type[]) {__VA_ARGS__}) / sizeof(Type)))
#define cbs_clear(da) \
	do { \
		if ((da)->items) free((da)->items); \
		cmd->count = cmd->capacity = 0; \
	} while(0)

void cbs_rebuild_self(char **argv);

char *cbs_shift_args(int *argc_p, char ***argv_p);
#define cbs_str_eq(str1, str2) strcmp(str1, str2) == 0
bool cbs_file_exists(char *file);
#define cbs_files_exist(file, ...) cbs_files_exist_nt(file, __VA_ARGS__, NULL)
#define cbs_needs_rebuild(target, ...) cbs_needs_rebuild_nt(target, __VA_ARGS__, NULL)
#define cbs_file_names_append(file_names, file_name) cbs_append_item(file_names, file_name)
Cbs_File_Names cbs_file_names_with_ext(char *dir_name, char *ext);

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
	if (*argc_p == 0) {
		return NULL;
	}
	--(*argc_p);
	return *((*argv_p)++);
}

bool cbs_file_exists(char *file) {
	struct stat temp;
	if (stat(file, &temp) == -1) {
		if (errno == ENOENT) {
			return false;
		}
		cbs_error("Could not access file");
	}
	return true;
}

bool cbs_files_exist_nt(char *file, ...) {
	if (!cbs_file_exists(file)) {
		return false;
	}
	va_list args;
	va_start(args, file);
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
	if (stat(target, &temp) == -1) {
		return true;
	}
	__darwin_time_t tar_mtime = temp.st_mtime;

	va_list args;
	va_start(args, target);

	char *dependency = va_arg(args, char *);
	while (dependency) {
		if (stat(dependency, &temp) == -1) {
			cbs_error("Could not open dependency when checking target rebuild");
		}
		__darwin_time_t dep_mtime = temp.st_mtime;
		if (tar_mtime < dep_mtime) {
			va_end(args);
			return true;
		}
		dependency = va_arg(args, char *);
	}

	va_end(args);
	return false;
}

static bool cbs_file_has_ext(char *file_name, char *ext) {
	if (file_name == NULL || ext == NULL) return false;
	char *file_name_p = file_name, *ext_p = ext;
	while (*file_name_p != '.' && *file_name_p != '\0') ++file_name_p;
	if (*file_name_p++ == '\0') return false;
	while (*file_name_p != '\0' && *ext_p != '\0') {
		if (*file_name_p++ != *ext_p++) return false;
	}
	if (*file_name_p != '\0' || *ext_p != '\0') return false;
	return true;
}

static char *cbs_file_strip_ext(char *file_name) {
	char *char_p = file_name;
	while (*char_p++ != '.');
	int file_name_len = char_p - file_name - 1;
	char *result = malloc((file_name_len + 1) * sizeof(char));
	strncpy(result, file_name, file_name_len);
	return result;
}

Cbs_File_Names cbs_file_names_with_ext(char *dir_name, char *ext) {
	Cbs_File_Names result = {0};
	DIR *dir = opendir(dir_name);
	struct dirent *entry = readdir(dir);
	do {
		char *file_name = entry->d_name;
		if (entry->d_type == DT_REG && cbs_file_has_ext(file_name, ext)) {
			cbs_file_names_append(&result, cbs_file_strip_ext(file_name));
		}
		entry = readdir(dir);
	} while(entry);
	closedir(dir);
	return result;
}

void cbs_cmd_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info proc = cbs_cmd_async_run(cmd);
	Cbs_Proc_Infos procs = {0};
	cbs_proc_infos_append(&procs, proc);
	cbs_async_wait(&procs);
}

static void cbs_file_print_to_stdout(FILE *file) {
	fseek(file, 0, SEEK_END);
	long file_size = ftell(file);
	if (file_size) {
		char *buffer = malloc(file_size * sizeof(char));
		if (buffer == NULL) cbs_error("Process ran out of memory");
		fseek(file, 0, SEEK_SET);
		fread(buffer, file_size, 1, file);
		fwrite(buffer, file_size, 1, stdout);
		free(buffer);
	}
}

bool cbs_cmd_try_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info proc = cbs_cmd_async_run(cmd);
	int status = 0;
	waitpid(proc.pid, &status, 0);
	cbs_cmd_print(proc.cmd);
	cbs_file_print_to_stdout(proc.output);
	if (status) cbs_log("Previous command ran unsuccessfully, continuing build");
	return !status;
}

static bool cbs_try_run_nt(char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs_cmd_append(&cmd, string);
	va_list args;
	va_start(args, string);
	char *arg = va_arg(args, char *);
	while (arg) {
		cbs_cmd_append(&cmd, arg);
		arg = va_arg(args, char *);
	}
	va_end(args);
	return cbs_cmd_try_run(&cmd);
}

void cbs_rebuild_self(char **argv) {
	char *file_name = argv[0], *backup_ext = ".bak";
	char *backup_file_name = calloc(strlen(file_name) + strlen(backup_ext), sizeof(char));
	strcpy(backup_file_name, file_name);
	strcat(backup_file_name, backup_ext);

	if (!cbs_needs_rebuild(file_name, "cbs.c", "cbs.h")) {
		return;
	}

	cbs_log("Rebuilding cbs");
	cbs_run("cp", file_name, backup_file_name);
	if (!cbs_try_run("cc", "-o", file_name, "cbs.c")) {
		cbs_log("Rebuild unsuccessful, undoing backup");
		cbs_run("cp", backup_file_name, file_name);
		cbs_run("rm", "-f", backup_file_name);
		cbs_error("Unable to rebuild cbs (bootstrapping may be necessary)");
	}
	cbs_run("rm", "-f", backup_file_name);
	cbs_log("Rebuild successful");
	if (execvp(file_name, argv) == -1) {
		cbs_error("Syntax error while running previous command, could not rebuild cbs");
	}
}

Cbs_Proc_Info cbs_cmd_async_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info result = {0};
	result.cmd = *cmd;
	result.cmd.items = malloc(cmd->capacity * sizeof(char *));
	memcpy(result.cmd.items, cmd->items, cmd->count * sizeof(char *));
	result.output = tmpfile();

	cbs_cmd_append(cmd, NULL);

	if ((result.pid = fork()) == 0) {
		dup2(fileno(result.output), STDOUT_FILENO);
		dup2(fileno(result.output), STDERR_FILENO);
		if (execvp(cmd->items[0], cmd->items) == -1) exit(1);
	}

	cbs_cmd_clear(cmd);
	return result;
}

static Cbs_Proc_Info cbs_async_run_nt(char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs_cmd_append(&cmd, string);
	va_list args;
	va_start(args, string);
	char *arg = va_arg(args, char *);
	while (arg) {
		cbs_cmd_append(&cmd, arg);
		arg = va_arg(args, char *);
	}
	va_end(args);
	return cbs_cmd_async_run(&cmd);
}

void cbs_async_wait(Cbs_Proc_Infos *procs) {
	if (procs == NULL || procs->count == 0) return;
	int status = 0;
	long file_size = 0;
	char *buffer;
	for (int i = 0; i < procs->count; ++i) {
		Cbs_Proc_Info proc = procs->items[i];
		waitpid(proc.pid, &status, 0);
		cbs_cmd_print(proc.cmd);
		cbs_file_print_to_stdout(proc.output);
		if (status) cbs_error("Previous command ran unsuccessfully, stopping build");
	}
	free(procs->items);
	procs->count = procs->capacity = 0;
}

#endif // CBS_IMPLEMENTATION
