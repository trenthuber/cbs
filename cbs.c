#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#define DYEXT ".dylib"
#define SEC st_mtimespec.tv_sec
#define NSEC st_mtimespec.tv_nsec
#else
#define DYEXT ".so"
#define SEC st_mtim.tv_sec
#define NSEC st_mtim.tv_nsec
#endif

#define NONE (char *[]){NULL}
#define LIST(...) (char *[]){__VA_ARGS__, NULL}

extern char **environ;

char **cflags = NONE, **lflags = NONE;

char *extend(char *path, char *altext) {
	char *dir, *base, *ext, *result;
	size_t d, b, l;

	base = (base = strrchr(dir = path, '/')) ? base + 1 : dir;
	d = base - dir;
	b = (ext = strrchr(base, '.')) ? ext - base : (ext = altext, strlen(base));
	if (altext[0] == '!') ext = altext + 1;
	if (strcmp(ext, DYEXT) == 0) {
		if (!(dir = realpath(path = d ? strndup(dir, d) : "./", NULL)))
			err(errno, "Unable to resolve `%s'", path);
		if (d) free(path);
		d = strlen(dir);
		dir[d++] = '/';
	}
	if (b > (l = 3) && strncmp(base, "lib", l) == 0
	    || strcmp(ext, ".a") != 0 && strcmp(ext, DYEXT) != 0)
		l = 0;

	if (!(result = calloc(d + l + b + strlen(ext) + 1, sizeof*result)))
		err(errno, "Memory allocation");
	strncat(result, dir, d);
	strncat(result, "lib", l);
	strncat(result, base, b);
	strcat(result, ext);

	if (strcmp(ext, DYEXT) == 0) free(dir);

	return result;
}

void run(char *file, char **args, char *what, char *who) {
	size_t i;

	if (file[0] != '!') for (i = 0; args[i]; ++i) {
		fputs(args[i], stdout);
		putchar(args[i + 1] ? ' ' : '\n');
	} else ++file;

	if (execve(file, args, environ) == -1)
		err(errno, "Unable to %s `%s'", what, who);
}

void await(pid_t cpid, char *what, char *who) {
	int status;

	if (cpid == -1 || waitpid(cpid, &status, 0) == -1)
		err(errno, "Unable to %s `%s'", what, who);
	if (WIFSIGNALED(status))
		errx(WTERMSIG(status), "%s", strsignal(WTERMSIG(status)));
	if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
		exit(WEXITSTATUS(status));
}

void compile(char *src) {
	size_t f;
	char **args, **p, *obj;
	pid_t cpid;

	for (f = 0; cflags[f]; ++f);
	if (!(p = args = calloc(2 + f + 3 + 1, sizeof*args)))
		err(errno, "Memory allocation");

	*p++ = "cc";
	*p++ = "-c";
	for (f = 0; cflags[f]; *p++ = cflags[f++]);
	*p++ = "-o";
	*p++ = obj = extend(src, "!.o");
	*p++ = src = extend(src, ".c");

	if ((cpid = fork()) == -1) err(errno, "Unable to fork");
	if (!cpid) run("/usr/bin/cc", args, "compile", src);
	await(cpid, "compile", src);

	free(src);
	free(obj);
	free(args);
}

void load(char type, char *target, char **objs) {
	size_t o, f;
	char **args, **p, **a, **fp, *path;
	pid_t cpid;

	for (o = 0; objs[o]; ++o);
	for (f = 0; lflags[f]; ++f);
	if (!(p = args = calloc(3 + o + 1 + f + 1, sizeof*args)))
		err(errno, "Memory allocation");
	fp = (a = p + 3) + o;

	switch (type) {
	case 'd':
		target = extend(target, DYEXT);
		--a;
		*fp++ = "-shared";
	case 'x':
		path = "/usr/bin/cc";
		*p++ = "cc";
		*p++ = "-o";
		break;
	case 's':
		path = "/usr/bin/ar";
		*p++ = "ar";
		*p++ = "-rc";
		target = extend(target, ".a");
		a = p = (fp = p) + f;
		break;
	default:
		errx(EXIT_FAILURE, "Unknown target type `%c'", type);
	}
	*p++ = target;
	for (o = 0; objs[o]; *p++ = extend(objs[o++], ".o"));
	for (f = 0; lflags[f]; *fp++ = lflags[f++]);

	if ((cpid = fork()) == -1) err(errno, "Unable to fork");
	if (!cpid) run(path, args, "link", target);
	await(cpid, "link", target);

	while (a < p) free(*a++);
	free(args);
}

void build(char *path) {
	char *absolute, *current;
	int self, exists, rebuild;
	pid_t cpid;
	struct stat src, exe;

	if (!(absolute = realpath(path, NULL)))
		err(errno, "Unable to resolve `%s'", path);
	if (!(current = getcwd(NULL, 0)))
		err(errno, "Unable to check current directory");

	if (!(self = strcmp(absolute, current) == 0)) {
		if ((cpid = fork()) == -1) err(errno, "Unable to fork");
		if (cpid) await(cpid, "run", "build");

		printf("cd %s/\n", path = cpid ? current : absolute);
		if (chdir(path) == -1) err(errno, "Unable to change directory to `%s'", path);
	} else cpid = 0;

	free(current);
	free(absolute);

	if (cpid) return;

	if (stat("build.c", &src) == -1) err(errno, "Unable to stat `build.c'");
	if (!(exists = stat("build", &exe) != -1)) exe.SEC = 0;
	rebuild = src.SEC == exe.SEC ? src.NSEC > exe.NSEC : src.SEC > exe.SEC;

	if (self ? rebuild : !exists) {
		compile("build");
		load('x', "build", LIST("build"));
	}
	if (!self || rebuild) run("!build", LIST("build"), "run", "build");

	if (utimensat(AT_FDCWD, "build.c", NULL, 0) == -1)
		err(errno, "Unable to update `build.c' modification time");
}
