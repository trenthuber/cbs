#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
#define DYEXT ".dylib"
#else
#define DYEXT ".so"
#endif

extern char **environ;

char **cflags, **lflags;

void *allocate(size_t s) {
	void *r;

	if (!(r = malloc(s))) err(EXIT_FAILURE, "Memory allocation");

	return memset(r, 0, s);
}

char *extend(char *path, char *ext) {
	char *dp, *bp, *e, *r;
	size_t d, b, l;

	if (!(dp = path)) return NULL;

	bp = (bp = strrchr(dp, '/')) ? bp + 1 : dp;
	d = bp - dp;
	b = (e = strrchr(bp, '.')) ? e - bp : (e = ext, strlen(bp));
	if (*ext == '!') e = ++ext;
	if (strcmp(e, DYEXT) == 0) {
		path = d ? strndup(dp, d) : strdup(".");
		if (!(dp = realpath(path, NULL)))
			err(EXIT_FAILURE, "Unable to get the absolute path of `%s'", path);
		free(path);
		d = strlen(dp);
		dp[d++] = '/';
	}
	l = (strcmp(e, ".a") == 0 || strcmp(e, DYEXT) == 0)
	    && (b <= 3 || strncmp(bp, "lib", 3) != 0) ? 3 : 0;

	r = allocate(d + l + b + strlen(e) + 1);
	strncat(r, dp, d);
	strncat(r, "lib", l);
	strncat(r, bp, b);
	strcat(r, e);

	if (dp != path) free(dp);

	return r;
}

void run(char *file, char **args, char *what, char *who) {
	size_t i;

	for (i = 0; args[i]; ++i) printf("%s ", args[i]);
	putchar('\n');

	if (execve(file, args, environ) == -1)
		err(EXIT_FAILURE, "Unable to %s `%s'", what, who);
}

void await(pid_t cpid, char *what, char *who) {
	int status;

	if (cpid == -1 || waitpid(cpid, &status, 0) == -1)
		err(EXIT_FAILURE, "Unable to %s `%s'", what, who);
	if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
		exit(EXIT_FAILURE);
}

void compile(char *src) {
	size_t f;
	char **args, **p;
	pid_t cpid;

	f = 0;
	if (cflags) while (cflags[f]) ++f;
	p = args = allocate((2 + f + 3 + 1) * sizeof*args);

	*p++ = "cc";
	*p++ = "-c";
	if (cflags) for (f = 0; cflags[f]; *p++ = cflags[f++]);
	*p++ = "-o";
	*p++ = extend(src, "!.o");
	*p++ = src = extend(src, ".c");

	if ((cpid = fork()) == 0) run("/usr/bin/cc", args, "compile", src);
	await(cpid, "compile", src);

	free(src);
	free(args);
}

void load(char type, char *target, char **objs) {
	size_t o, f;
	char **args, **p, **a, **fp, *path;
	pid_t cpid;

	f = o = 0;
	if (objs) while (objs[o]) ++o;
	if (lflags) while (lflags[f]) ++f;
	args = allocate((3 + o + 1 + f + 1) * sizeof*args);
	fp = (a = (p = args) + 3) + o;

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
		*p++ = "-r";
		target = extend(target, ".a");
		a = p = (fp = p) + f;
		break;
	default:
		errx(EXIT_FAILURE, "Unknown target type `%c'", type);
	}
	*p++ = target;
	f = o = 0;
	if (objs) while (objs[o]) *p++ = extend(objs[o++], ".o");
	if (lflags) while (lflags[f]) *fp++ = lflags[f++];

	if ((cpid = fork()) == 0) run(path, args, "link", target);
	await(cpid, "link", target);

	while (a < p) free(*a++);
	free(args);
}

void build(char *path) {
	pid_t cpid;
	struct stat src, obj;

	if (path) {
		if ((cpid = fork())) {
			await(cpid, "run", "build");
			puts("cd ..");
			return;
		}
		printf("cd %s\n", path);
		if (chdir(path))
			err(EXIT_FAILURE, "Unable to change directory to `%s'", path);
	}

	if (stat("build.c", &src) == -1)
		err(EXIT_FAILURE, "Unable to stat `build.c'");
	if (stat("build.o", &obj) == -1 && src.st_mtime > obj.st_mtime) {
		compile("build");
		load('x', "build", (char *[]){"build", NULL});
	} else if (!path) return;

	run("build", (char *[]){"build", NULL}, "run", "build");
}
