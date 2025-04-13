#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

char **cflags, **lflags;

void error(char *fmt, ...) {
	va_list args;

	dprintf(STDERR_FILENO, "cbs: ");
	va_start(args, fmt);
	vdprintf(STDERR_FILENO, fmt, args);
	va_end(args);
	if (errno) dprintf(STDERR_FILENO, ": %s", strerror(errno));
	dprintf(STDERR_FILENO, "\n");

	exit(EXIT_FAILURE);
}

void *alloc(size_t size) {
	void *r;

	if (!(r = malloc(size))) error("Memory allocation");

	return r;
}

char *extend(char *path, char *ext) {
	char *bp, *ep, *rp, *tp;
	int d, b, e, l;

	if (!path) return NULL;

	bp = (bp = strrchr(path, '/')) ? bp + 1 : path;
	d = bp - path;
	b = (ep = strrchr(bp, '.')) ? ep - bp : (ep = ext, strlen(bp));
	if (*ext == '!') ep = ext + 1;
	e = strlen(ep);
	l = strncmp(ep, ".a", e) == 0 || strncmp(ep, ".dylib", e) == 0 ? 3 : 0;

	tp = rp = alloc(d + l + b + e + 1);
	tp = stpncpy(tp, path, d);
	tp = stpncpy(tp, "lib", l);
	tp = stpncpy(tp, bp, b);
	stpncpy(tp, ep, e + 1);

	return rp;
}

int modified(char *target, char *dep) {
	struct stat tstat, dstat;

	if (stat(target, &tstat) == -1 && errno != ENOENT)
		error("Unable to stat `%s'", target);
	if (stat(dep, &dstat) == -1) error("Unable to stat `%s'", dep);

	return errno == ENOENT || tstat.st_mtime < dstat.st_mtime;
}

void run(char *path, char **args, char *what, char *who) {
	int i;

	for (i = 0; args[i]; ++i) printf("%s ", args[i]);
	printf("\n");

	if (execve(path, args, environ) == -1)
		error("Unable to run %s of `%s'", what, who);
}

void await(int cpid, char *what, char *who) {
	int status;

	if (cpid == -1) error("Unable to delegate the %s of `%s'", what, who);
	if (waitpid(cpid, &status, 0) == -1)
		error("Unable to await the %s of `%s'", what, who);
	if (WIFEXITED(status) && WEXITSTATUS(status) != EXIT_SUCCESS)
		exit(EXIT_FAILURE);
}

void compile(char *src, ...) {
	int fn, cpid;
	char **args, **p, *obj, *dep;
	va_list deps;

	fn = 0;
	if (cflags) while (cflags[fn]) ++fn;
	p = args = alloc((5 + fn + 1) * sizeof*args);

	*p++ = "cc";
	if (cflags) for (fn = 0; cflags[fn]; *p++ = cflags[fn++]);
	*p++ = "-c";
	*p++ = "-o";
	*p++ = obj = extend(src, "!.o");
	*p++ = src = extend(src, ".c");
	*p = NULL;

	dep = strdup(src);
	va_start(deps, src);
	do if (modified(obj, dep)) {
		if ((cpid = fork()) == 0) run("/usr/bin/cc", args, "compilation", src);
		await(cpid, "compilation", src);
		break;
	} while (free(dep), dep = extend(va_arg(deps, char *), ".h"));
	va_end(deps);

	free(src);
	free(obj);
	free(args);
}

void load(char type, char *target, char *obj, ...) {
	int fn, vn, cpid;
	va_list count, objs;
	char **args, **p, *path, **o;

	fn = 0;
	if (lflags) while (lflags[fn]) ++fn;
	va_start(count, obj);
	va_copy(objs, count);
	for (vn = 0; va_arg(count, char *); ++vn);
	va_end(count);
	p = args = alloc((5 + fn + vn + 1) * sizeof*args);

	path = "/usr/bin/cc";
	*p++ = "cc";
	switch (type) {
	case 'd':
		*p++ = "-dynamiclib";
	case 'x':
		if (lflags) for (fn = 0; lflags[fn]; *p++ = lflags[fn++]);
		*p++ = "-o";
		*p++ = target = type == 'd' ? extend(target, ".dylib") : strdup(target);
		break;
	case 's':
		path = "/usr/bin/ar";
		*(p - 1) = "ar";
		*p++ = "-r";
		*p++ = target = extend(target, ".a");
		break;
	default:
		error("Unknown linking type `%c'", type);
	}
	o = p;
	*o++ = extend(obj, ".o");
	while ((*o++ = extend(va_arg(objs, char *), ".o")));
	va_end(objs);

	o = p;
	while (*o) if (modified(target, *o++)) {
		if ((cpid = fork()) == 0) run(path, args, "linking", target);
		await(cpid, "linking", target);
		break;
	}

	while (*p) free(*p++);
	free(target);
	free(args);
}

void build(char *path) {
	int cpid;

	if (path) {
		if ((cpid = fork())) {
			await(cpid, "execution", "build");
			printf("cd ..\n");
			return;
		}
		printf("cd %s\n", path);
		if (chdir(path)) error("Unable to set working directory to `%s'", path);
	}

	if (modified("build", "build.c")) {
		if ((cpid = fork()) == 0)
			run("/usr/bin/cc", (char *[]){"cc", "-o", "build", "build.c", NULL},
			    "compilation", "build.c");
		await(cpid, "compilation", "build.c");
	} else if (!path) return;

	run("build", (char *[]){"./build", NULL}, "execution", "build");
}
