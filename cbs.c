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
} Command;

void command_print(Command command) {
	for (int i = 0; i < command.count; ++i) {
		printf("\"%s\",\n", command.items[i]);
	}
	printf("count: %d\n", command.count);
	printf("cap: %d\n", command.capacity);
}

void command_append(Command *command, char *string) {
	if (command->capacity == 0) {
		if ((command->items = malloc(sizeof(char *))) == NULL) {
			perror("process ran out of memory");
			exit(1);
		}
		command->capacity = 1;
	}
	if (command->count >= command->capacity) {
		if ((command->items = realloc(command->items, 2 * command->capacity * sizeof(char *))) == NULL) {
			perror("process ran out of memory");
			exit(1);
		}
		command->capacity *= 2;
	}
	if (string == NULL) {
		command->items[command->count++] = NULL;
		return;
	}
	command->items[command->count] = malloc(strlen(string));
	strcpy(command->items[command->count++], string);
}

#define command_multi_append(command, ...) command_multi_append_null_term(command, __VA_ARGS__, NULL)

void command_multi_append_null_term(Command *command, ...) {
	va_list args;
	va_start(args, command);

	char *next_arg = va_arg(args, char *);
	while(next_arg) {
		command_append(command, next_arg);
		next_arg = va_arg(args, char *);
	}

	va_end(args);
}

void command_clear(Command *command) {
	for (int i = 0; i < command->count; ++i) {
		if (command->items[i])
			free(command->items[i]);
	}
	if (command->items)
		free(command->items);
	command->count = command->capacity = 0;
}

bool command_execute(Command *command) {
	pid_t pid = fork();
	if (pid == 0) {
		if (execvp(command->items[0], command->items) == -1) {
			perror("execve failed");
			exit(1);
		}
	}
	int status = 0;
	wait(&status);
	command_clear(command);
	return status == 0;
}

#define cbs_run(...) \
	do { \
		Command cmd = {0}; \
		command_multi_append(&cmd, __VA_ARGS__); \
		command_append(&cmd, NULL); \
		if (!command_execute(&cmd)) { \
			fprintf(stderr, "could not run command at %s(%u)\n", __FILE__, __LINE__); \
			exit(1); \
		}; \
	} while(0)

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

	Command cmd = {0};
	cbs_run("cp", filename, filename_bak);
	command_multi_append(&cmd, "cc", "-o", filename, filename_c);
	command_append(&cmd, NULL);
	if (!command_execute(&cmd)) {
		printf("Recompile unsuccessful\n");
		cbs_run("cp", filename_bak, filename);
		cbs_run("rm", "-f", filename_bak);
		exit(1);
	} else {
		printf("Recompile successful\n");
		cbs_run("rm", "-f", filename_bak);
		command_multi_append(&cmd, filename);
		command_append(&cmd, NULL);
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
