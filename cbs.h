/* cbs, Trent Huber 2024
 * https://github.com/trenthuber/cbs.git
 *
 *
 * SUMMARY
 *
 * cbs is a build system for C written in C (although it can certainly be used
 * to build more than just C projects). While most modern day build systems are
 * far more feature rich than this project, they lack at least one thing: a
 * fully developed and mature build language. That's where cbs comes in. In
 * essence, build scripts written with cbs are only constrained by the
 * capabilities of the C programming language itself, not some superficially
 * created one. While perhaps more verbose than others, C's capabilites as a
 * "real" and fully-fleshed out programming langauge make it worth the effort in
 * the end. To make the process that much easier, much of the functionality that
 * is common in building projects has been wrapped up in this single-file
 * library, including running Bourne shell commands, navigating file trees,
 * extracting names and extensions of files, and even multi-process building
 * (similar to the -j option in Make).
 *
 * See README.md for details on getting started.
 *
 *
 * INTERFACE OVERVIEW
 *
 * Here is a list of all the functions available to you. Note that the actual
 * implementation details are not important here (i.e., some of these
 * "functions" are really macros, but that doesn't matter much). They've been
 * organized in the order they would be "typically" used in a build program.
 *
 * RET    NAME                          PARAMETERS
 *
 * - Self rebuilding -
 * void   cbs_rebuild_self              (char *const *argv); [^1]
 *
 * - General purpose -
 * void   cbs_log                       (const char *string, (const char *)...);
 * void   cbs_error                     (const char *string, (const char *)...);
 * char * cbs_shift_args                (int *argc_p, char ***argv_p);
 * bool   cbs_string_eq                 (const char *string1,
 *                                       const char *string2);
 * char * cbs_string_build              (const char *string1,
 *                                       const char *string2,
 *                                       (const char *)...);
 * char * cbs_get_file_dir              (const char *file_path);
 * char * cbs_get_file_name             (const char *file_path);
 * char * cbs_get_file_ext              (const char *file_path);
 * char * cbs_strip_file_ext            (const char *file_path);
 * void   cbs_cd                        (const char *dir);
 *
 * - File paths -
 * void   cbs_file_paths_build          (Cbs_File_Paths *file_paths,
 *                                       Cbs_File_Path file_path,
 *                                       (Cbs_File_Path)...);
 * void   cbs_file_paths_build_file_ext (Cbs_File_Paths *file_paths,
 *                                       const char *dir_path, const char *ext);
 * void   cbs_file_paths_for_each       (Cbs_File_Path file_path,
 *                                       Cbs_File_Paths *file_paths);
 *
 * - File caching -
 * bool   cbs_files_exist               (const char *file_path,
 *                                       (const char *)...);
 * bool   cbs_needs_rebuild             (const char *target, const char *dep,
 *                                       (const char *)...);
 * bool   cbs_needs_rebuild_file_paths  (const char *target,
 *                                       Cbs_File_Paths deps);
 *
 * - Build and run commands -
 * void   cbs_cmd_build                 (Cbs_Cmd *cmd, const char *string,
 *                                       (const char *)...);
 * void   cbs_cmd_build_file_paths      (Cbs_Cmd *cmd,
 *                                       Cbs_File_Paths file_paths);
 * void   cbs_cmd_print                 (Cbs_Cmd *cmd);
 * int    cbs_cmd_run_status            (Cbs_Cmd *cmd);
 * void   cbs_cmd_run                   (Cbs_Cmd *cmd);
 *
 * - Run commands directly -
 * int    cbs_run_status                (const char *string, (const char *)...);
 * void   cbs_run                       (const char *string, (const char *)...);
 *
 * - Run commands asynchronously -
 * void   cbs_cmd_async_run             (Cbs_Async_Procs *procs, Cbs_Cmd *cmd);
 * void   cbs_async_run                 (Cbs_Async_Procs *procs,
 *                                       const char *string, (const char *)...);
 * void   cbs_async_wait                (Cbs_Async_Procs *procs);
 *
 * - Subbuild -
 * void   cbs_subbuild                  (const char *dir,
 *                                       (const char *)...); [^2]
 *
 * [^1] cbs_rebuild_self() assumes your source file is cbs.c in the same
 * directory as the currently running file is in. Thus, this function is only
 * accessable to call if you've defined CBS_IMPLEMENTATION. Further more, you
 * can define CBS_LIBRARY_PATH with the path of the cbs.h file you're including
 * so it automatically rebuilds when that file is edited (default value is
 * "./cbs.h").
 *
 * [^2] cbs_subbuild() assumes your source file is cbs.c in the directory
 * specified and will create the binary file, labeled cbs, in that directory.
 * Variadics passed to it are interpreted as subcommands for that build file.
 *
 *
 * LICENSE
 *
 * Copyright (c) 2024 Trent Huber
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _CBS_H_
#define _CBS_H_

#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

// Internal macros
#define cbs__da_init_cap 16
#define cbs__indent "  "
#define cbs__malloc_error \
	fprintf(stderr, cbs__indent "MALLOC ERROR: Process ran out of memory\n")
#define cbs__da_struct(Type, Name) \
	typedef struct { \
		Type *items; \
		size_t count; \
		size_t cap; \
	} Name
#define cbs__append_item(da, item) \
	do { \
		if ((da)->cap == 0) { \
			(da)->cap = cbs__da_init_cap; \
			if (((da)->items = malloc(sizeof(*(da)->items))) == NULL) \
				cbs__malloc_error; \
		} else if ((da)->count >= (da)->cap) { \
			(da)->cap *= 2; \
			(da)->items = realloc((da)->items, (da)->cap * sizeof(*(da)->items)); \
			if ((da)->items == NULL) cbs__malloc_error; \
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
			if ((da)->items == NULL) cbs__malloc_error; \
		} \
		memcpy(&(da)->items[(da)->count], list, size * sizeof(*(da)->items)); \
		(da)->count += size; \
	} while(0)
#define cbs__append_items(Type, da, ...) \
	cbs__append_list(da, ((Type[]){__VA_ARGS__}), \
	                 (size_t)(sizeof((Type[]){__VA_ARGS__}) / sizeof(Type)))
#define cbs__clear(da) (da)->count = 0

// Utility functions: logging, parsing the command line, modifying file paths
#define cbs_log(...) \
	printf(cbs__indent "  LOG: %s\n", cbs_string_build("", __VA_ARGS__))
#define cbs_error(...) \
	exit((fprintf(stderr, cbs__indent "ERROR: %s (%s:%u)\n", \
	              cbs_string_build("", __VA_ARGS__), __FILE__, __LINE__), \
	      errno ? (perror(cbs__indent " INFO"), errno) : EXIT_FAILURE))
char *cbs_shift_args(int *argc_p, char ***argv_p);
#define cbs_string_eq(string1, string2) strcmp(string1, string2) == 0
#define cbs_string_build(string, ...) \
	cbs__string_build_nt(string, __VA_ARGS__, NULL)
char *cbs_get_file_dir(const char *file_path);
char *cbs_get_file_name(const char *file_path);
char *cbs_get_file_ext(const char *file_path);
char *cbs_strip_file_ext(const char *file_path);
#define cbs_cd(dir) \
	if (printf(cbs__indent "cd %s\n", dir), chdir(dir)) \
		cbs_error("Unable to enter directory ", dir)

// File paths
cbs__da_struct(const char *, Cbs_File_Paths);
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

// File caching conditionals
#define cbs_files_exist(...) \
	cbs__files_exist_nt(__VA_ARGS__, NULL)
#define cbs_needs_rebuild(target, ...) \
	cbs__needs_rebuild_nt(target, __VA_ARGS__, NULL)
bool cbs_needs_rebuild_file_paths(const char *target, Cbs_File_Paths deps);

// Build and run commands
cbs__da_struct(const char *, Cbs_Cmd);
#define cbs_cmd_build(cmd, ...) \
	cbs__append_items(const char *, cmd, __VA_ARGS__)
#define cbs_cmd_build_file_paths(cmd, file_paths) \
	if (file_paths.count > 0) \
		cbs_file_paths_for_each(file_path, file_paths) cbs_cmd_build(cmd, file_path)
#define cbs_cmd_print(cmd) \
	do { \
		printf(cbs__indent); \
		for (size_t i = 0; i < (cmd).count - 1; ++i) printf("%s ", (cmd).items[i]); \
		printf("%s\n", (cmd).items[(cmd).count - 1]); \
	} while(0)
int cbs_cmd_run_status(Cbs_Cmd *cmd);
#define cbs_cmd_run(cmd) \
	if (cbs_cmd_run_status(cmd)) \
		cbs_error("Previous command ran unsuccessfully, stopping build");

// Run commands directly
#define cbs_run_status(...) cbs__run_status_nt(__VA_ARGS__, NULL)
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

// Run a subbuild
#define cbs_subbuild(...) cbs__subbuild_nt(__VA_ARGS__, NULL)

#endif // _CBS_H_

#ifdef CBS_IMPLEMENTATION

#ifndef CBS_LIBRARY_PATH
#define CBS_LIBRARY_PATH "./cbs.h"
#endif // CBS_LIBRARY_PATH

#include <dirent.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/wait.h>

char *cbs_shift_args(int *argc_p, char ***argv_p) {
	if (*argc_p <= 0) return NULL;
	(*argc_p)--;
	return *((*argv_p)++);
}

char *cbs__string_build_nt(const char *string1, const char *string2, ...) {
	size_t sum_len = strlen(string1) + strlen(string2);

	va_list args;
	va_start(args, string2);
	const char *arg;
	while ((arg = va_arg(args, const char *))) sum_len += strlen(arg);
	va_end(args);

	char *result = malloc((sum_len + 1) * sizeof(char));
	if (!result) cbs__malloc_error;
	*result = '\0';
	strncat(result, string1, strlen(string1));
	strncat(result, string2, strlen(string2));

	va_start(args, string2);
	while ((arg = va_arg(args, const char *)))
		strncat(result, arg, strlen(arg));
	va_end(args);

	return result;
}

char *cbs_get_file_dir(const char *file_path) {
	const char *const index = rindex(file_path, '/');
	char *result = index ? strndup(file_path, index - file_path) : "";
	if (!result) cbs__malloc_error;
	return result;
}

char *cbs_get_file_name(const char *file_path) {
	const char *const index = rindex(file_path, '/');
	char *result = index ? strndup(file_path + (index - file_path + 1),
	                               strlen(file_path) - (index - file_path + 1))
	                     : strndup(file_path, strlen(file_path));
	if (!result) cbs__malloc_error;
	return result;
}

char *cbs_get_file_ext(const char *file_path) {
	const char *const index = rindex(file_path, '.'),
	           *const slash_index = rindex(file_path, '/');
	if (slash_index && index < slash_index) return "";
	char *result = index ? strndup(file_path + (index - file_path),
	                               strlen(file_path) - (index - file_path))
	                     : strndup(file_path, strlen(file_path));
	if (!result) cbs__malloc_error;
	return result;
}

char *cbs_strip_file_ext(const char *file_path) {
	const size_t file_ext_len = strlen(cbs_get_file_ext(file_path));
	char *result = file_ext_len > 0
	             ? strndup(file_path, strlen(file_path) - file_ext_len)
	             : strndup(file_path, strlen(file_path));
	if (!result) cbs__malloc_error;
	return result;
}

void cbs_file_paths_build_file_ext(Cbs_File_Paths *file_paths,
                                   const char *dir_path,
                                   const char *ext) {
	DIR *const dir = opendir(dir_path);
	if (!dir) cbs_error("Unable to open ", dir_path, " for search");
	struct dirent *entry;
	errno = 0;
	while ((entry = readdir(dir)))
		if (entry->d_type == DT_REG
		    && cbs_string_eq(cbs_get_file_ext(entry->d_name), ext))
			cbs_file_paths_build(file_paths, cbs_string_build(dir_path, "/",
			                                                  entry->d_name));
	if (errno) cbs_error("Unable to access entry ", dir_path, "/", entry->d_name);
	if (closedir(dir)) cbs_error("Unable to close ", dir_path, " after search");
}

bool cbs__files_exist_nt(const char *file_path, ...) {
	struct stat temp;
	if (stat(file_path, &temp)) {
		if (errno == ENOENT) return false;
		cbs_error("Unable to access file ", file_path);
	}

	va_list args;
	va_start(args, file_path);
	const char *arg;
	while ((arg = va_arg(args, const char *)))
		if (stat(arg, &temp)) {
			if (errno == ENOENT) {
				va_end(args);
				return false;
			}
			cbs_error("Unable to access file ", arg);
		}
	va_end(args);
	return true;
}

bool cbs__needs_rebuild_nt(const char *target, const char *dep, ...) {
	if (!cbs_files_exist(target)) return true;

	struct stat temp;
	stat(target, &temp);
	const size_t target_mtime = (const size_t)temp.st_mtime;

	if (!cbs_files_exist(dep))
		cbs_error("Could not open ", dep, " when checking target rebuild");

	stat(dep, &temp);
	if (target_mtime < (const size_t)temp.st_mtime) return true;

	va_list args;
	va_start(args, dep);
	const char *current_dep;
	while ((current_dep = va_arg(args, const char *))) {
		if (!cbs_files_exist(current_dep))
			cbs_error("Could not open ", current_dep, " when checking target rebuild");

		stat(current_dep, &temp);
		if (target_mtime < (const size_t)temp.st_mtime) {
			va_end(args);
			return true;
		}
	}
	va_end(args);
	return false;
}

bool cbs_needs_rebuild_file_paths(const char *target, Cbs_File_Paths deps) {
	if (!cbs_files_exist(target)) return true;

	struct stat temp;
	stat(target, &temp);
	const size_t target_mtime = (const size_t)temp.st_mtime;

	const char *dep;
	for (size_t i = 0; i < deps.count; ++i) {
		dep = deps.items[i];
		if (!cbs_files_exist(dep))
			cbs_error("Could not open ", dep, " when checking target rebuild");

		stat(dep, &temp);
		if (target_mtime < (const size_t)temp.st_mtime) return true;
	}
	return false;
}

int cbs_cmd_run_status(Cbs_Cmd *cmd) {
	cbs_cmd_print(*cmd);
	cbs_cmd_build(cmd, NULL);
	pid_t child_pid;
	if ((child_pid = fork()) == 0
	    && (execvp(cmd->items[0], (char *const *)cmd->items)))
		cbs_error("Unable to run command, check command syntax");
	cbs__clear(cmd);

	int stat;
	waitpid(child_pid, &stat, 0);
	return WIFEXITED(stat) ? WEXITSTATUS(stat) : -1;
}

int cbs__run_status_nt(const char *string, ...) {
	Cbs_Cmd cmd = {0};
	cbs__append_item(&cmd, string);
	va_list args;
	va_start(args, string);
	const char *arg;
	while ((arg = va_arg(args, const char *))) cbs__append_item(&cmd, arg);
	va_end(args);
	return cbs_cmd_run_status(&cmd);
}

void cbs_rebuild_self(char *const *argv) {
	const char *const current_file_path = argv[0],
	           *const current_dir = cbs_get_file_dir(current_file_path),
	           *const src_file_path = cbs_string_build(current_dir, "/cbs.c");
	if (!cbs_needs_rebuild(current_file_path, src_file_path, CBS_LIBRARY_PATH))
		return;
	cbs_log("Rebuilding ", current_file_path);
	if (cbs_run_status("cc", "-Wall", "-Wextra", "-Wpedantic",
	                   "-o", current_file_path, src_file_path))
		cbs_error("Rebuild unsuccessful, bootstrapping may be necessary");
	cbs_log("Rebuild successful");
	if (execvp(current_file_path, argv))
		cbs_error("Rebuilt build command ran unsuccessfully, "
		          "bootstrapping may be necessary");
}

void cbs_cmd_async_run(Cbs_Async_Procs *procs, Cbs_Cmd *cmd) {
	Cbs__Async_Proc proc = {0};
	proc.cmd = *cmd;
	proc.cmd.items = malloc(cmd->cap * sizeof(char *));
	if (!proc.cmd.items) cbs__malloc_error;
	memcpy(proc.cmd.items, cmd->items, cmd->count * sizeof(char *));
	proc.output = tmpfile();

	cbs_cmd_build(cmd, NULL);
	if ((proc.pid = fork()) == 0) {
		dup2(fileno(proc.output), STDOUT_FILENO);
		dup2(fileno(proc.output), STDERR_FILENO);
		if (execvp(cmd->items[0], (char *const *)cmd->items))
			cbs_error("Unable to run command, check command syntax");
	}
	cbs__clear(cmd);

	cbs__append_item(procs, proc);
}

void cbs__file_print(FILE *file, FILE *output) {
	if (fseek(file, 0, SEEK_END))
		cbs_error("Unable to seek file during print");
	const long file_size = ftell(file);
	if (file_size == -1) cbs_error("Unable to access file during print");
	if (file_size == 0) return;
	if (fseek(file, 0, SEEK_SET))
		cbs_error("Unable to seek file during print");

	char *const buffer = malloc((file_size + 1) * sizeof(char));
	if (!buffer) cbs__malloc_error;
	size_t file_index = file_size, n;
	char *buffer_p = buffer;
	while(file_index > 0) {
		n = fread(buffer_p, 1, file_index, file);
		if (ferror(file)) cbs_error("Unable to read from file while printing");
		file_index -= n;
		buffer_p += n;
	}
	*buffer_p = '\0';
	fprintf(output, "%s\n", buffer);
	free(buffer);
}

void cbs_async_wait(Cbs_Async_Procs *procs) {
	if (procs == NULL || procs->count == 0) return;
	int stat, status;
	for (size_t i = 0; i < procs->count; ++i) {
		Cbs__Async_Proc proc = procs->items[i];
		waitpid(proc.pid, &stat, 0);
		cbs_cmd_print(proc.cmd);

		status = WIFEXITED(stat) ? WEXITSTATUS(stat) : -1;
		cbs__file_print(proc.output, status == 0 ? stdout : stderr);
		if (status) cbs_error("Previous command ran unsuccessfully, stopping build");
	}
	cbs__clear(procs);
}

void cbs__subbuild_nt(const char *dir, ...) {
	const char *const src_file_path = cbs_string_build(dir, "/cbs.c"),
	           *const bin_file_path = cbs_strip_file_ext(src_file_path);
	if (!cbs_files_exist(bin_file_path))
		cbs_run("cc", "-o", bin_file_path, src_file_path);

	char *const cwd = getcwd(NULL, 0);
	cbs_cd(dir);

	Cbs_Cmd cmd = {0};
	cbs_cmd_build(&cmd, cbs_string_build("./", cbs_get_file_name(bin_file_path)));
	va_list args;
	va_start(args, dir);
	const char *arg;
	while ((arg = va_arg(args, const char *))) cbs_cmd_build(&cmd, arg);
	va_end(args);
	cbs_cmd_run(&cmd);

	cbs_cd(cwd);
	free(cwd);
}

#endif // CBS_IMPLEMENTATION
