#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

#define CARRAY(x) (char *[]){x, NULL}
#define CC(...) cc(__VA_ARGS__, NULL)
#define LD(...) ld(__VA_ARGS__, NULL)

extern char **environ;

void *alloc(int s) {
	void *r;

	if ((r = malloc(s))) return r;

	fprintf(stderr, "Out of memory\n");
	exit(EXIT_FAILURE);
}

char *addext(char *pp, char *ext, int lprf) {
	char *bp, *ep, *rp, *tp;
	int d, l, b, e;

	if (pp == NULL) return NULL;

	bp = rindex(pp, '/');
	bp = bp ? bp + 1 : pp;
	d = bp - pp;
	l = lprf && strncmp(bp, "lib", 3) != 0 ? 3 : 0;
	ep = rindex(bp, '.');
	b = ep ? ep - bp : strlen(bp);
	ep = ep ? ep + 1 : ext;
	e = strlen(ep);
	
	rp = alloc(d + l + b + 1 + e + 1);
	
	tp = strncpy(rp, pp, d);
	tp = strncpy(tp + d, "lib", l);
	tp = strncpy(tp + l, bp, b);
	tp = strncpy(tp + b, ".", 1);
	strncpy(tp + 1, ep, e + 1);

	return rp;
}

int checkmod(char *targ, char *dep) {
	struct stat dstat, tstat;

	if (stat(dep, &dstat) == -1) {
		fprintf(stderr, "Unable to stat `%s': %s\n", dep, strerror(errno));
		exit(EXIT_FAILURE);
	}
	errno = 0;
	if (stat(targ, &tstat) == -1 && errno != ENOENT) {
		fprintf(stderr, "Unable to stat `%s': %s\n", targ, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return errno == ENOENT || dstat.st_mtime > tstat.st_mtime;
}

void exec(char *path, char **args, char *what, char *who) {
	int i;

	for (i = 0; args[i]; ++i) if (*args[i] != '\0') printf("%s ", args[i]);
	printf("\n");

	if (execve(path, args, environ) == -1) {
		fprintf(stderr, "Unable to run %s of `%s'\n", what, who);
		exit(EXIT_FAILURE);
	}
}

void pwait(int cpid, char *what, char *who) {
	int status;

	if (cpid == -1) {
		fprintf(stderr, "Unable to delegate %s of `%s'\n", what, who);
		exit(EXIT_FAILURE);
	}
	waitpid(cpid, &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0) exit(EXIT_FAILURE);
}

void cc(char *name, ...) {
	char *dep, *src, *obj;
	va_list deps;
	int cpid;

	dep = src = addext(name, "c", 0);
	obj = addext(name, "o", 0);

	va_start(deps, name);
	do if (checkmod(obj, dep)) {
		if ((cpid = fork()) == 0)
			exec("/usr/bin/cc", (char *[]){"cc", CFLAGS, "-c", "-o", obj, src, NULL},
			     "compilation", src);
		pwait(cpid, "compilation", src);
		break;
	} while ((dep = addext(va_arg(deps, char *), "h", 0)));
	va_end(deps);
}

void ld(char type, char *output, char *input, ...) {
	char **fargs, **args;
	int fn, vn, i, cpid;
	va_list count, inputs;

	input = addext(input, "o", 0);
	switch (type) {
	case 'x':
		fargs = (char *[]){"cc", LDFLAGS, "-o", output, input, NULL};
		break;
	case 's':
		output = addext(output, "a", 1);
		fargs = (char *[]){"ar", "-r", output, input, NULL};
		break;
	case 'd':
		output = addext(output, "dylib", 1);
		fargs = (char *[]){"cc", "-dynamiclib", LDFLAGS, "-o", output, input, NULL};
		break;
	default:
		fprintf(stderr, "Unknown linking type `%c'\n", type);
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
		args[i] = addext(va_arg(inputs, char *), "o", 0);
	args[fn + vn] = NULL;
	va_end(inputs);

	for (i = fn - 1; i < fn + vn; ++i) if (checkmod(output, args[i])) {
		if ((cpid = fork()) == 0)
			exec(type == 's' ? "/usr/bin/ar" : "/usr/bin/cc",
			     args, "linking phase", output);
		pwait(cpid, "linking phase", output);
		break;
	}
}

void build(char *path) {
	int cpid;

	if (path) {
		if ((cpid = fork())) {
			pwait(cpid, "execution", "build");
			printf("cd ..\n");
			return;
		}
		printf("cd %s\n", path);
		if (chdir(path))
			fprintf(stderr, "Unable to make `%s' the current working directory: %s\n",
			        path, strerror(errno));
	}

	if (checkmod("build", "build.c")) {
		if ((cpid = fork()) == 0)
			exec("/usr/bin/cc", (char *[]){"cc", "-o", "build", "build.c", NULL},
			     "compilation", "build.c");
		pwait(cpid, "compilation", "build.c");
	} else if (!path) return;

	exec("build", (char *[]){"build", NULL}, "execution", "build");
}
