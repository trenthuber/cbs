#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

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

bool command_execute(Command command) {
	// printf("ABOUT TO EXECUTE THIS:\n");
	// command_print(command);
	// command.items[command.count] = NULL;
	// command_append(&command, NULL);
	pid_t pid = fork();
	if (pid == 0) { // Child process
		if (execvp(command.items[0], command.items) == -1) {
			perror("execve failed");
			exit(1);
		}
	}
	int status = 0;
	wait(&status);
	return status == 0;
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

int main(int argc, char **argv) {
	char *filename = argv[0];
	char *filename_bak = calloc(strlen(filename) + 5, sizeof(char));
	strcpy(filename_bak, filename);
	strcat(filename_bak, ".bak");
	char *filename_c = calloc(strlen(filename) + 3, sizeof(char));
	strcpy(filename_c, filename);
	strcat(filename_c, ".c");
	// printf("filename_bak: %s, filename_c: %s\n", filename_bak, filename_c);
	printf("Hello, seaman!\n");

	// Check if we need to rebuild this file
#ifndef _DARWIN_FEATURE_64_BIT_INODE
#define _DARWIN_FEATURE_64_BIT_INODE
#endif
	struct stat my_stat;
	stat(filename, &my_stat);
	__darwin_time_t mtime = my_stat.st_mtime;
	stat(filename_c, &my_stat);
	__darwin_time_t c_mtime = my_stat.st_mtime;
	if (mtime > c_mtime) {
		return 0;
	}

	Command cmd = {0};
	command_append(&cmd, "cp");
	command_append(&cmd, filename);
	command_append(&cmd, filename_bak);
	command_append(&cmd, NULL);
	command_print(cmd);
	if (!command_execute(cmd)) {
		fprintf(stderr, "unable to create backup file");
		exit(1);
	}
	command_clear(&cmd);
	command_append(&cmd, "cc");
	command_append(&cmd, "-o");
	command_append(&cmd, filename);
	command_append(&cmd, filename_c);
	command_append(&cmd, NULL);
	command_print(cmd);
	if (!command_execute(cmd)) {
		printf("Recompile unsuccessful\n");
		command_clear(&cmd);
		command_append(&cmd, "cp");
		command_append(&cmd, filename_bak);
		command_append(&cmd, filename);
		command_append(&cmd, NULL);
		if (!command_execute(cmd)) {
			fprintf(stderr, "unable to copy backup file back");
			exit(1);
		}
		command_clear(&cmd);
		command_append(&cmd, "rm");
		command_append(&cmd, "-f");
		command_append(&cmd, filename_bak);
		command_append(&cmd, NULL);
		if (!command_execute(cmd)) {
			fprintf(stderr, "unable to remove backup file");
			exit(1);
		}
		return 1;
	} else {
		printf("Recompile successful\n");
		command_clear(&cmd);
		command_append(&cmd, "rm");
		command_append(&cmd, "-f");
		command_append(&cmd, filename_bak);
		command_append(&cmd, NULL);
		if (!command_execute(cmd)) {
			fprintf(stderr, "unable to remove backup file");
			exit(1);
		}
		command_clear(&cmd);
		command_append(&cmd, filename);
		command_append(&cmd, NULL);
		command_print(cmd);
		if (execvp(cmd.items[0], cmd.items) == -1) {
			perror("could not rebuild project");
			exit(1);
		}
	}
	
	return 0;
}
