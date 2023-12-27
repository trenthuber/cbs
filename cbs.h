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

#endif // _CBS_H_

#ifdef CBS_IMPLEMENTATION

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
	long filesize = ftell(file);
	if (filesize) {
		char *buffer = malloc(filesize * sizeof(char));
		if (buffer == NULL) cbs_error("Process ran out of memory");
		fseek(file, 0, SEEK_SET);
		fread(buffer, filesize, 1, file);
		fwrite(buffer, filesize, 1, stdout);
		free(buffer);
	}
}

bool cbs_cmd_try_run(Cbs_Cmd *cmd) {
	Cbs_Proc_Info proc = cbs_cmd_async_run(cmd);
	int status = 0;
	waitpid(proc.pid, &status, 0);
	cbs_cmd_print(proc.cmd);
	if (status) cbs_log("Previous command ran unsuccessfully, continuing build");
	else cbs_file_print_to_stdout(proc.output);
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
	long filesize = 0;
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
	char *filename = argv[0], *backup_ext = ".bak";
	char *backup_filename = calloc(strlen(filename) + strlen(backup_ext), sizeof(char));
	strcpy(backup_filename, filename);
	strcat(backup_filename, backup_ext);

	if (!cbs_needs_rebuild(filename, "cbs.c", "cbs.h")) {
		return;
	}

	cbs_log("Rebuilding cbs");
	cbs_run("cp", filename, backup_filename);
	if (!cbs_try_run("cc", "-o", filename, "cbs.c")) {
		cbs_log("Rebuild unsuccessful, undoing backup");
		cbs_run("cp", backup_filename, filename);
		cbs_run("rm", "-f", backup_filename);
		cbs_error("Unable to rebuild cbs (bootstrapping may be necessary)");
	}
	cbs_run("rm", "-f", backup_filename);
	cbs_log("Rebuild successful");
	if (execvp(filename, argv) == -1) {
		cbs_error("Syntax error while running previous command, could not rebuild cbs");
	}
}

#endif // CBS_IMPLEMENTATION
