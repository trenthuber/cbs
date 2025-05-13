#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
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

void await(pid_t cpid, char *what, char *who) {
	int status;

	if (cpid == -1 || waitpid(cpid, &status, 0) == -1)
		err(EXIT_FAILURE, "Unable to %s `%s'", what, who);
	if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
		exit(EXIT_FAILURE);
}

int modified(char *target, char *dep) {
	char *ext;
	struct stat tstat, dstat;

	if ((ext = strrchr(dep, '.')) && strcmp(ext, DYEXT) == 0) return 0;

	if (stat(target, &tstat) == -1) {
		if (errno == ENOENT) return 1;
		err(EXIT_FAILURE, "Unable to stat `%s'", target);
	}
	if (stat(dep, &dstat) == -1) err(EXIT_FAILURE, "Unable to stat `%s'", dep);

	return tstat.st_mtime < dstat.st_mtime;
}

void run(char *path, char **args, char *what, char *who) {
	size_t i;

	for (i = 0; args[i]; ++i) printf("%s ", args[i]);
	putchar('\n');

	if (execve(path, args, environ) == -1)
		err(EXIT_FAILURE, "Unable to %s `%s'", what, who);
}

void build(char *path) {
	pid_t cpid;

	if (path) {
		if ((cpid = fork())) {
			await(cpid, "run", "build");
			puts("cd ..");
			return;
		}
		printf("cd %s\n", path);
		if (chdir(path))
			err(EXIT_FAILURE, "Unable to set working directory to `%s'", path);
	}

	if (modified("build", "build.c")) {
		if ((cpid = fork()) == 0)
			run("/usr/bin/cc", (char *[]){"cc", "-o", "build", "build.c", NULL},
			    "compile", "build.c");
		await(cpid, "compile", "build.c");
	} else if (!path) return;

	run("build", (char *[]){"./build", NULL}, "run", "build");
}

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
		dp[(d = strlen(dp))] = '/';
		++d;
	}
	l = strcmp(e, ".a") == 0 || strcmp(e, DYEXT) == 0 ? 3 : 0;

	r = allocate(d + l + b + strlen(e) + 1);
	strncat(r, dp, d);
	strncat(r, "lib", l);
	strncat(r, bp, b);
	strcat(r, e);

	if (dp != path) free(dp);

	return r;
}

void compile(char *src, ...) {
	size_t f;
	char **args, **p, *obj, *hdr;
	va_list hdrs;
	pid_t cpid;

	if (f = 0, cflags) while (cflags[f]) ++f;
	p = args = allocate((5 + f + 1) * sizeof*args);

	*p++ = "cc";
	*p++ = "-c";
	if (cflags) for (f = 0; cflags[f]; *p++ = cflags[f++]);
	*p++ = "-o";
	*p++ = obj = extend(src, "!.o");
	*p++ = hdr = src = extend(src, ".c");

	va_start(hdrs, src);
	do if (modified(obj, hdr = extend(hdr, ".h"))) {
		if ((cpid = fork()) == 0) run("/usr/bin/cc", args, "compile", src);
		await(cpid, "compile", src);
		break;
	} while (free(hdr), hdr = va_arg(hdrs, char *));
	va_end(hdrs);

	free(src);
	free(obj);
	free(args);
}

void load(char type, char *target, char *obj, ...) {
	va_list count, objs;
	size_t o, f;
	char **args, **p, **a, **fp, *path;
	pid_t cpid;

	va_start(count, obj);
	va_copy(objs, count);
	for (o = 1; va_arg(count, char *); ++o);
	va_end(count);
	if (f = 0, lflags) while (lflags[f]) ++f;
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
		fp = p;
		a = p += f;
		break;
	default:
		errx(EXIT_FAILURE, "Unknown target type `%c'", type);
	}
	*p++ = target;
	do *p++ = extend(obj, ".o"); while ((obj = va_arg(objs, char *)));
	va_end(objs);
	if (lflags) for (f = 0; lflags[f]; *fp++ = lflags[f++]);
	fp = p;

	p -= o;
	while (o--) if (modified(target, *p++)) {
		if ((cpid = fork()) == 0) run(path, args, "link", target);
		await(cpid, "link", target);
		break;
	}

	while (a < fp) free(*a++);
	free(args);
}
