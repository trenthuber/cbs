#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

extern char **environ;

void *alloc(int s) {
	void *r;

	if ((r = malloc(s))) return r;

	perror(NULL);
	exit(EXIT_FAILURE);
}

char *extend(char *path, char *ext, int lib) {
	char *bp, *rp, *tp;
	int d, l, b, e;

	if (path == NULL) return NULL;

	bp = rindex(path, '/');
	bp = bp ? bp + 1 : path;
	d = bp - path;
	l = lib ? 3 : 0;
	b = strlen(bp);
	e = strlen(ext);

	tp = rp = alloc(d + l + b + e + 1);
	tp = stpncpy(tp, path, d);
	tp = stpncpy(tp, "lib", l);
	tp = stpncpy(tp, bp, b);
	stpncpy(tp, ext, e + 1);

	return rp;
}

int modified(char *tpath, char *dpath) {
	struct stat dstat, tstat;

	if (stat(dpath, &dstat) == -1) {
		fprintf(stderr, "Unable to stat `%s': %s\n", dpath, strerror(errno));
		exit(EXIT_FAILURE);
	}
	errno = 0;
	if (stat(tpath, &tstat) == -1 && errno != ENOENT) {
		fprintf(stderr, "Unable to stat `%s': %s\n", tpath, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return errno == ENOENT || dstat.st_mtime > tstat.st_mtime;
}

void run(char *path, char **args, char *what, char *who) {
	int i;

	for (i = 0; args[i]; ++i) if (*args[i] != '\0') printf("%s ", args[i]);
	printf("\n");

	if (execve(path, args, environ) == -1) {
		fprintf(stderr, "Unable to run the %s of `%s': %s\n",
		        what, who, strerror(errno));
		exit(EXIT_FAILURE);
	}
}

void await(int cpid, char *what, char *who) {
	int status;

	if (cpid == -1) {
		fprintf(stderr, "Unable to delegate the %s of `%s': %s\n",
		        what, who, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (waitpid(cpid, &status, 0) == -1) {
		fprintf(stderr, "Unable to await the %s of %s: %s\n",
		        what, who, strerror(errno));
		exit(EXIT_FAILURE);
	}
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0) exit(EXIT_FAILURE);
}

void compile(char *name, ...) {
	char *src, *dep, *obj;
	va_list deps;
	int cpid;

	dep = src = extend(name, ".c", 0);
	obj = extend(name, ".o", 0);

	va_start(deps, name);
	do if (modified(obj, dep)) {
		if ((cpid = fork()) == 0)
			run("/usr/bin/cc", (char *[]){"cc", CFLAGS, "-c", "-o", obj, src, NULL},
			    "compilation", src);
		await(cpid, "compilation", src);
		break;
	} while ((dep = extend(va_arg(deps, char *), ".h", 0)));
	va_end(deps);
}

void load(char type, char *output, char *input, ...) {
	char **fargs, **args;
	int fn, vn, i, cpid;
	va_list count, inputs;

	input = extend(input, ".o", 0);
	switch (type) {
	case 'x':
		fargs = (char *[]){"cc", LFLAGS, "-o", output, input, NULL};
		break;
	case 's':
		output = extend(output, ".a", 1);
		fargs = (char *[]){"ar", "-r", output, input, NULL};
		break;
	case 'd':
		output = extend(output, ".dylib", 1);
		fargs = (char *[]){"cc", "-dynamiclib", LFLAGS, "-o", output, input, NULL};
		break;
	default:
		fprintf(stderr, "Unknown load type `%c'\n", type);
		exit(EXIT_FAILURE);
	}
	for (fn = 0; fargs[fn]; ++fn);

	va_start(count, input);
	va_copy(inputs, count);
	for (vn = 0; va_arg(count, char *); ++vn);
	va_end(count);

	args = alloc((fn + vn + 1) * sizeof(char *));
	memcpy(args, fargs, fn * sizeof(char *));
	for (i = fn; i < fn + vn; ++i)
		args[i] = extend(va_arg(inputs, char *), ".o", 0);
	args[fn + vn] = NULL;
	va_end(inputs);

	for (i = fn - 1; i < fn + vn; ++i) if (modified(output, args[i])) {
		if ((cpid = fork()) == 0)
			run(type == 's' ? "/usr/bin/ar" : "/usr/bin/cc",
			    args, "loading", output);
		await(cpid, "loading", output);
		break;
	}
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
		if (chdir(path)) {
			fprintf(stderr, "Unable to make `%s' the current working directory: %s\n",
			        path, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	if (modified("build", "build.c")) {
		if ((cpid = fork()) == 0)
			run("/usr/bin/cc", (char *[]){"cc", "-o", "build", "build.c", NULL},
			    "compilation", "build.c");
		await(cpid, "compilation", "build.c");
	} else if (!path) return;

	run("build", (char *[]){"build", NULL}, "execution", "build");
}
