#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void *allocate(size_t s) {
	void *r;

	if (!(r = malloc(s))) err(EXIT_FAILURE, "Memory allocation");

	return memset(r, 0, s);
}

char *extend(char *path, char *ext) {
	char *bp, *e, *dp, *r;
	size_t b, d, l;

	bp = (bp = strrchr(path, '/')) ? bp + 1 : path;
	b = (e = strrchr(bp, '.')) ? e - bp : (e = ext, strlen(bp));
	if (*ext == '!') e = ext + 1;
	d = bp - path;
	if (strcmp(e, DYEXT) == 0) {
		if (!(dp = realpath(path = d ? strndup(path, d) : strdup("./"), NULL)))
			err(EXIT_FAILURE, "Unable to get the absolute path of `%s'", path);
		free(path);
		d = strlen(dp);
		dp[d++] = '/';
	} else dp = strdup(path);
	l = (strcmp(e, ".a") == 0 || strcmp(e, DYEXT) == 0)
	    && (b <= 3 || strncmp(bp, "lib", 3) != 0) ? 3 : 0;

	r = allocate(d + l + b + strlen(e) + 1);
	strncat(r, dp, d);
	strncat(r, "lib", l);
	strncat(r, bp, b);
	strcat(r, e);

	free(dp);

	return r;
}

void run(char *file, char **args, char *what, char *who) {
	size_t i;

	if (*file != '!') for (i = 0; args[i]; ++i) {
		fputs(args[i], stdout);
		putchar(args[i + 1] ? ' ' : '\n');
	} else ++file;

	if (execve(file, args, environ) == -1)
		err(EXIT_FAILURE, "Unable to %s `%s'", what, who);
}

void await(pid_t cpid, char *what, char *who) {
	int status;

	if (cpid == -1 || waitpid(cpid, &status, 0) == -1)
		err(EXIT_FAILURE, "Unable to %s `%s'", what, who);
	if (WIFSIGNALED(status)) errx(EXIT_FAILURE, "%s", strsignal(WTERMSIG(status)));
	if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
		exit(EXIT_FAILURE);
}

void compile(char *src) {
	size_t f;
	char **args, **p, *obj;
	pid_t cpid;

	for (f = 0; cflags[f]; ++f);
	p = args = allocate((2 + f + 3 + 1) * sizeof*args);

	*p++ = "cc";
	*p++ = "-c";
	for (f = 0; cflags[f]; *p++ = cflags[f++]);
	*p++ = "-o";
	*p++ = obj = extend(src, "!.o");
	*p++ = src = extend(src, ".c");

	if ((cpid = fork()) == -1) err(EXIT_FAILURE, "Unable to fork");
	if (!cpid) run("/usr/bin/cc", args, "compile", src);
	await(cpid, "compile", src);

	free(obj);
	free(src);
	free(args);
}

void load(char type, char *target, char **objs) {
	size_t o, f;
	char **args, **p, **a, **fp, *path;
	pid_t cpid;

	for (o = 0; objs[o]; ++o);
	for (f = 0; lflags[f]; ++f);
	p = args = allocate((3 + o + 1 + f + 1) * sizeof*args);
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

	if ((cpid = fork()) == -1) err(EXIT_FAILURE, "Unable to fork");
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
		err(EXIT_FAILURE, "Unable to resolve `%s'", path);
	if (!(current = getcwd(NULL, 0)))
		err(EXIT_FAILURE, "Unable to check current directory");

	if (!(self = strcmp(absolute, current) == 0)) {
		if ((cpid = fork()) == -1) err(EXIT_FAILURE, "Unable to fork");
		if (cpid) await(cpid, "run", "build");

		printf("cd %s/\n", path = cpid ? current : absolute);
		if (chdir(path) == -1)
			err(EXIT_FAILURE, "Unable to change directory to `%s'", path);
	} else cpid = 0;

	free(absolute);
	free(current);

	if (cpid) return;

	if (stat("build.c", &src) == -1) err(EXIT_FAILURE, "Unable to stat `build.c'");
	if (!(exists = stat("build", &exe) != -1)) exe.SEC = 0;
	rebuild = src.SEC == exe.SEC ? src.NSEC > exe.NSEC : src.SEC > exe.SEC;

	if (!self && !exists || self && rebuild) {
		compile("build");
		load('x', "build", LIST("build"));
	}
	if (!self || rebuild) run("!build", LIST("build"), "run", "build");

	if (utimensat(AT_FDCWD, "build.c", NULL, 0) == -1)
		err(EXIT_FAILURE, "Unable to update `build.c' modification time");
}
