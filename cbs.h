#ifndef _CBS_H_
#define _CBS_H_

typedef struct {
	char **items;
	int count;
	int capacity;
} Cbs_Cmd;

#define CBS_LOG(x) printf("%s\n", (x))
#define CBS_ERROR(x) \
	do { \
		fprintf(stderr, "\nERROR: %s (%s:%u)\n", (x), __FILE__, __LINE__); \
		if (errno) perror("INFO"); \
		fprintf(stderr, "\n"); \
		exit(1); \
	} while(0)

#define cbs_str_eq(str1, str2) strcmp(str1, str2) == 0
char *cbs_shift_args(int *argc_p, char ***argv_p);
void cbs_cmd_print(Cbs_Cmd cmd);
void cbs_cmd_clear(Cbs_Cmd *cmd);
void cbs_cmd_append(Cbs_Cmd *cmd, char *string);
#define cbs_cmd_build(cmd, ...) cbs_cmd_build_nt(cmd, __VA_ARGS__, NULL)
int cbs_cmd_run(Cbs_Cmd *cmd);
static Cbs_Cmd dummy_cmd = {0};
#define cbs_run(...) (cbs_cmd_build(&dummy_cmd, __VA_ARGS__), cbs_cmd_run(&dummy_cmd))
#define cbs_needs_rebuild(target, ...) cbs_needs_rebuild_nt(target, __VA_ARGS__, NULL)
void cbs_rebuild_self(int argc, char **argv);

#endif // _CBS_H_

#ifdef CBS_IMPLEMENTATION

#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

char *cbs_shift_args(int *argc_p, char ***argv_p) {
	if (*argc_p == 0) {
		return NULL;
	}
	--(*argc_p);
	return *((*argv_p)++);
}

void cbs_cmd_print(Cbs_Cmd cmd) {
	printf("  ");
	for (int i = 0; i < cmd.count - 1; ++i) {
		printf("%s ", cmd.items[i]);
	}
	printf("%s\n", cmd.items[cmd.count - 1]);
}

void cbs_cmd_clear(Cbs_Cmd *cmd) {
	for (int i = 0; i < cmd->count; ++i) {
		if (cmd->items[i])
			free(cmd->items[i]);
	}
	if (cmd->items) free(cmd->items);
	cmd->count = cmd->capacity = 0;
}

void cbs_cmd_append(Cbs_Cmd *cmd, char *string) {
	if (cmd->capacity == 0) {
		if ((cmd->items = malloc(sizeof(char *))) == NULL)
			CBS_ERROR("Process ran out of memory");
		cmd->capacity = 1;
	}
	if (cmd->count >= cmd->capacity) {
		if ((cmd->items = realloc(cmd->items, 2 * cmd->capacity * sizeof(char *))) == NULL)
			CBS_ERROR("Process ran out of memory");
		cmd->capacity *= 2;
	}
	if (string == NULL) {
		cmd->items[cmd->count++] = NULL;
		return;
	}
	cmd->items[cmd->count] = malloc(strlen(string));
	strcpy(cmd->items[cmd->count++], string);
}

static void cbs_cmd_build_nt(Cbs_Cmd *cmd, ...) {
	va_list args;
	va_start(args, cmd);

	char *next_arg = va_arg(args, char *);
	while(next_arg) {
		cbs_cmd_append(cmd, next_arg);
		next_arg = va_arg(args, char *);
	}

	va_end(args);
}

int cbs_cmd_run(Cbs_Cmd *cmd) {
	cbs_cmd_print(*cmd);
	cbs_cmd_append(cmd, NULL);
	pid_t pid = fork();
	if (pid == 0) {
		if (execvp(cmd->items[0], cmd->items) == -1) {
			kill(getppid(), SIGKILL);
			CBS_ERROR("Syntax error while running previous command");
		}
	}
	int status = 0;
	wait(&status);
	cbs_cmd_clear(cmd);
	return status;
}

static bool cbs_needs_rebuild_nt(char *target, ...) {
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
			CBS_ERROR("Could not open dependency when checking target rebuild");
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

void cbs_rebuild_self(int argc, char **argv) {
	(void) argc;

	char *backup_ext = ".bak", *c_ext = ".c", *h_ext = ".h", *filename = argv[0];

	char *backup_filename = calloc(strlen(filename) + strlen(backup_ext), sizeof(char));
	strcpy(backup_filename, filename);
	strcat(backup_filename, backup_ext);
	char *c_filename = calloc(strlen(filename) + strlen(c_ext), sizeof(char));
	strcpy(c_filename, filename);
	strcat(c_filename, c_ext);
	char *h_filename = calloc(strlen(filename) + strlen(c_ext), sizeof(char));
	strcpy(h_filename, filename);
	strcat(h_filename, h_ext);

	if (!cbs_needs_rebuild(filename, c_filename, h_filename)) {
		return;
	}

	CBS_LOG("Rebuilding cbs");
	cbs_run("cp", filename, backup_filename);
	if (cbs_run("cc", "-o", filename, c_filename) != 0) {
		CBS_LOG("Rebuild unsuccessful, undoing backup");
		cbs_run("cp", backup_filename, filename);
		cbs_run("rm", "-f", backup_filename);
		CBS_ERROR("Unable to rebuild cbs (bootstrapping may be necessary)");
	}
	cbs_run("rm", "-f", backup_filename);
	CBS_LOG("Rebuild successful");
	if (execvp(filename, argv) == -1) {
		CBS_ERROR("Syntax error while running previous command, could not rebuild cbs");
	}
}

#endif // CBS_IMPLEMENTATION
