#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct {
	char **items;
	int count;
	int capacity;
} Cbs_Cmd;

void cbs_cmd_print(Cbs_Cmd cmd) {
	for (int i = 0; i < cmd.count; ++i) {
		printf("\"%s\",\n", cmd.items[i]);
	}
	printf("count: %d\n", cmd.count);
	printf("cap: %d\n", cmd.capacity);
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
		if ((cmd->items = malloc(sizeof(char *))) == NULL) {
			perror("process ran out of memory");
			exit(1);
		}
		cmd->capacity = 1;
	}
	if (cmd->count >= cmd->capacity) {
		if ((cmd->items = realloc(cmd->items, 2 * cmd->capacity * sizeof(char *))) == NULL) {
			perror("process ran out of memory");
			exit(1);
		}
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

#define cbs_cmd_build(cmd, ...) cbs_cmd_build_nt(cmd, __VA_ARGS__, NULL)

int cbs_cmd_run(Cbs_Cmd *cmd) {
	cbs_cmd_append(cmd, NULL);
	pid_t pid = fork();
	if (pid == 0) {
		if (execvp(cmd->items[0], cmd->items) == -1) {
			kill(getppid(), SIGKILL);
			fprintf(stderr, "cbs did not understand this command: \n");
			cbs_cmd_print(*cmd);
			exit(1);
		}
	}
	int status = 0;
	wait(&status);
	cbs_cmd_clear(cmd);
	return status;
}

static Cbs_Cmd global_cmd = {0};

#define cbs_run(...) (cbs_cmd_build(&global_cmd, __VA_ARGS__), cbs_cmd_run(&global_cmd))

void cbs_rebuild(int argc, char **argv) {
	(void) argc;
	char *filename = argv[0];
	char *filename_bak = calloc(strlen(filename) + 5, sizeof(char));
	strcpy(filename_bak, filename);
	strcat(filename_bak, ".bak");
	char *filename_c = calloc(strlen(filename) + 3, sizeof(char));
	strcpy(filename_c, filename);
	strcat(filename_c, ".c");

	// Check if we need to rebuild this file
	struct stat my_stat;
	stat(filename, &my_stat);
	__darwin_time_t mtime = my_stat.st_mtime;
	stat(filename_c, &my_stat);
	__darwin_time_t c_mtime = my_stat.st_mtime;
	if (mtime > c_mtime) {
		return;
	}

	cbs_run("cp", filename, filename_bak);
	if (cbs_run("cc", "-o", filename, filename_c) != 0) {
		printf("Recompile unsuccessful\n");
		cbs_run("cp", filename_bak, filename);
		cbs_run("rm", "-f", filename_bak);
		exit(1);
	} else {
		printf("Recompile successful\n");
		cbs_run("rm", "-f", filename_bak);
		Cbs_Cmd cmd = {0};
		cbs_cmd_append(&cmd, filename);
		cbs_cmd_append(&cmd, NULL);
		if (execvp(cmd.items[0], cmd.items) == -1) {
			perror("could not rebuild project");
			exit(1);
		}
	}
}

int main(int argc, char **argv) {
	cbs_rebuild(argc, argv);

	printf("Hello, world!\n");

	return 0;
}
