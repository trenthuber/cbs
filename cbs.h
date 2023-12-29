#ifndef _CBS_H_
#define _CBS_H_

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

typedef struct {
	char **items;
	int count;
	int capacity;
} Cbs_Cmd;

typedef struct {
	Cbs_Cmd cmd;
	FILE *output;
	pid_t pid;
} Cbs_Proc_Info;

typedef struct {
	Cbs_Proc_Info *items;
	int count;
	int capacity;
} Cbs_Proc_Infos;

#define cbs_log(x) printf("%s\n", x)
#define cbs_error(x) \
	do { \
		fprintf(stderr, "\nERROR: %s (%s:%u)\n", x, __FILE__, __LINE__); \
		if (errno) perror("INFO"); \
		fprintf(stderr, "\n"); \
		exit(1); \
	} while(0)

void cbs_rebuild_self(char **argv);
char *cbs_shift_args(int *argc_p, char ***argv_p);
#define cbs_str_eq(str1, str2) strcmp(str1, str2) == 0
bool cbs_file_exists(char *file);
#define cbs_files_exist(file, ...) cbs_files_exist_nt(file, __VA_ARGS__, NULL)
#define cbs_needs_rebuild(target, ...) cbs_needs_rebuild_nt(target, __VA_ARGS__, NULL)

// TODO: Polish dynamic arrays in the whole thingy
// TODO: cbs_merge that merges dynamic arrays?

void cbs_cmd_append(Cbs_Cmd *cmd, char *string);
#define cbs_cmd_build(cmd, ...) cbs_cmd_build_nt(cmd, __VA_ARGS__, NULL)
void cbs_cmd_print(Cbs_Cmd cmd);
void cbs_cmd_clear(Cbs_Cmd *cmd);
bool cbs_cmd_try_run(Cbs_Cmd *cmd);
void cbs_cmd_run(Cbs_Cmd *cmd);
#define cbs_try_run(...) (cbs_cmd_build(&dummy_cmd, __VA_ARGS__), cbs_cmd_try_run(&dummy_cmd))
#define cbs_run(...) \
	do { \
		cbs_cmd_build(&dummy_cmd, __VA_ARGS__); \
		cbs_cmd_run(&dummy_cmd); \
	} while(0)
Cbs_Proc_Info cbs_cmd_async_run(Cbs_Cmd *cmd);
#define cbs_async_run(...) (cbs_cmd_build(&dummy_cmd, __VA_ARGS__), cbs_cmd_async_run(&dummy_cmd))
void cbs_proc_infos_append(Cbs_Proc_Infos *procs, Cbs_Proc_Info proc);
#define cbs_proc_infos_append_many(procs, ...) cbs_proc_infos_append_many_zt(procs, __VA_ARGS__, (Cbs_Proc_Info) {0})
void cbs_async_wait(Cbs_Proc_Infos *procs);

typedef struct {
	char **items;
	int count;
	int capacity;
} Cbs_File_Names;

Cbs_File_Names cbs_file_names_with_ext(char *dir_name, char *ext);

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

Cbs_Cmd dummy_cmd = {0};

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

void cbs_cmd_append(Cbs_Cmd *cmd, char *string) {
	if (cmd->capacity == 0) {
		if ((cmd->items = malloc(sizeof(*cmd->items))) == NULL)
			cbs_error("Process ran out of memory");
		cmd->capacity = 1;
	}
	if (cmd->count >= cmd->capacity) {
		if ((cmd->items = realloc(cmd->items, 2 * cmd->capacity * sizeof(*cmd->items))) == NULL)
			cbs_error("Process ran out of memory");
		cmd->capacity *= 2;
	}
	cmd->items[cmd->count++] = string;
}

void cbs_cmd_build_nt(Cbs_Cmd *cmd, ...) {
	va_list args;
	va_start(args, cmd);

	char *next_arg = va_arg(args, char *);
	while(next_arg) {
		cbs_cmd_append(cmd, next_arg);
		next_arg = va_arg(args, char *);
	}

	va_end(args);
}

void cbs_cmd_print(Cbs_Cmd cmd) {
	printf("  ");
	for (int i = 0; i < cmd.count - 1; ++i) {
		printf("%s ", cmd.items[i]);
	}
	printf("%s\n", cmd.items[cmd.count - 1]);
}

void cbs_cmd_clear(Cbs_Cmd *cmd) {
	if (cmd->items) free(cmd->items);
	cmd->count = cmd->capacity = 0;
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

void cbs_proc_infos_append(Cbs_Proc_Infos *procs, Cbs_Proc_Info proc) {
	if (procs->capacity == 0) {
		if ((procs->items = malloc(sizeof(Cbs_Proc_Info))) == NULL)
			cbs_error("Process ran out of memory");
		procs->capacity = 1;
	}
	if (procs->count >= procs->capacity) {
		if ((procs->items = realloc(procs->items, 2 * procs->capacity * sizeof(*procs->items))) == NULL)
			cbs_error("Process ran out of memory");
		procs->capacity *= 2;
	}
	procs->items[procs->count++] = proc;
}

void cbs_proc_infos_append_many_zt(Cbs_Proc_Infos *procs, ...) {
	va_list args;
	va_start(args, procs);
	Cbs_Proc_Info proc = va_arg(args, Cbs_Proc_Info);
	while (proc.output) {
		cbs_proc_infos_append(procs, proc);
		proc = va_arg(args, Cbs_Proc_Info);
	}
	va_end(args);
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

void cbs_file_names_append(Cbs_File_Names *file_names, char *file_name) {
	if (file_names->capacity == 0) {
		if ((file_names->items = malloc(sizeof(*file_names->items))) == NULL)
			cbs_error("Process ran out of memory");
		file_names->capacity = 1;
	}
	if (file_names->count >= file_names->capacity) {
		if ((file_names->items = realloc(file_names->items, 2 * file_names->capacity * sizeof(*file_names->items))) == NULL)
			cbs_error("Process ran out of memory");
		file_names->capacity *= 2;
	}
	file_names->items[file_names->count++] = file_name;
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

#endif // CBS_IMPLEMENTATION
