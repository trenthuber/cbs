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

	if ((r = malloc(s)) == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(EXIT_FAILURE);
	}
	return r;
}

char *addext(char *pp, char *ext) {
	char *bp, *ep, *rp, *tp;
	int d, b, l, e;

	if (pp == NULL) return NULL;

	bp = rindex(pp, '/');
	bp = bp ? bp + 1 : pp;
	d = bp - pp;
	ep = rindex(pp, '.');
	if (ep < bp) ep = NULL;
	b = ep ? ep - bp : strlen(bp);
	ep = ep ? ep + 1 : ext;
	l = strncmp(bp, "lib", 3) != 0
	    && (strncmp(ep, "a", 1) == 0
	        || strncmp(ep, "dylib", 5) == 0)
	    ? 3 : 0;
	e = strlen(ep);
	
	rp = alloc(d + l + b + 1 + e + 1);
	
	tp = strncpy(rp, pp, d);
	tp = strncpy(tp + d, "lib", l);
	tp = strncpy(tp + l, bp, b);
	tp = strncpy(tp + b, ".", 1);
	strncpy(tp + 1, ep, e + 1);

	return rp;
}

int checkmod(char *tar, char *dep) {
	struct stat dstat, tstat;

	if (stat(dep, &dstat) == -1) {
		fprintf(stderr, "Unable to stat `%s': %s\n", dep, strerror(errno));
		exit(EXIT_FAILURE);
	}
	errno = 0;
	if ((stat(tar, &tstat) == -1) && (errno != ENOENT)) {
		fprintf(stderr, "Unable to stat `%s': %s\n", tar, strerror(errno));
		exit(EXIT_FAILURE);
	}
	return (errno == ENOENT) || (dstat.st_mtime > tstat.st_mtime);
}

void exec(char *path, char **args, char *emsg, char *etar) {
	int i;

	for (i = 0; args[i]; ++i)
		if (*args[i] != '\0') printf("%s ", args[i]);
	printf("\n");

	if (execve(path, args, environ) == -1) {
		fprintf(stderr, emsg, etar);
		exit(EXIT_FAILURE);
	}
}

void pwait(int cpid, char *emsg, char *etar) {
	int status;

	if (cpid == -1) {
		fprintf(stderr, emsg, etar);
		exit(EXIT_FAILURE);
	}
	waitpid(cpid, &status, 0);
	if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
		exit(EXIT_FAILURE);
}

void cc(char *name, ...) {
	int cpid, status;
	char *src, *obj, *dep;
	va_list deps;

	if ((cpid = fork()) == 0) {
		src = addext(name, "c");
		obj = addext(name, "o");

		va_start(deps, name);
		dep = src;
		do
			if (checkmod(obj, dep))
				exec("/usr/bin/cc", (char *[]){"cc", CFLAGS, "-c", "-o", obj, src, NULL},
				     "Unable to execute compilation of `%s.c'\n", name);
		while ((dep = addext(va_arg(deps, char *), "h")));
		va_end(deps);

		exit(EXIT_SUCCESS);
	}

	pwait(cpid, "Unable to delegate compilation of `%s.c'\n", name);
}

void ld(char type, char *output, char *input, ...) {
	int cpid, status, vn, fn, i;
	va_list count, inputs;
	char **fargs, **args;

	switch (type) {
	case 'x':
		break;
	case 's':
	case 'd':
		output = addext(output, type == 's' ? "a" : "dylib");
		break;
	default:
		fprintf(stderr, "Unknown library type `%c'\n", type);
		exit(EXIT_FAILURE);
	}

	if ((cpid = fork()) == 0) {
		va_start(count, input);
		va_copy(inputs, count);
		for (vn = 0; va_arg(count, char *); ++vn);
		va_end(count);

		input = addext(input, "o");

		switch (type) {
		case 'x':
			fargs = (char *[]){"cc", LDFLAGS, "-o", output, input, NULL};
			break;
		case 's':
			fargs = (char *[]){"ar", "-r", output, input, NULL};
			break;
		case 'd':
			fargs = (char *[]){"cc", "-dynamiclib", LDFLAGS, "-o", output, input, NULL};
			break;
		}

		for (fn = 0; fargs[fn]; ++fn);

		args = alloc((fn + vn + 1) * sizeof(char *));
		memcpy(args, fargs, fn * sizeof(char *));
		for (i = fn; i < fn + vn; ++i)
			args[i] = addext(va_arg(inputs, char *), "o");
		args[fn + vn] = NULL;
		va_end(inputs);

		for (i = fn - 1; i < fn + vn; ++i)
			if (checkmod(output, args[i]))
				exec(type == 's' ? "/usr/bin/ar" : "/usr/bin/cc", args,
				     "Unable to execute linking phase of `%s'\n", output);

		exit(EXIT_SUCCESS);
	}

	pwait(cpid, "Unable to delegate linking phase of `%s'\n", output);
}

void build(char *path) {
	int pid;

	if (!path) {
		if (!checkmod("build", "build.c")) return;
		if ((pid = fork()) == 0)
			exec("/usr/bin/cc", (char *[]){"cc", "-o", "build", "build.c", NULL},
				 "Unable to execute compilation of `%s.c'\n", "build");
		pwait(pid, "Unable to delegate compilation of `%s.c'\n", "build");
		exec("build", (char *[]){"build", NULL}, "Unable to execute `%s'\n", "build");
	}

	if ((pid = fork()) == 0) {
		printf("cd %s\n", path);
		if (chdir(path))
			fprintf(stderr, "Unable to make `%s' the current working directory: %s\n",
			        path, strerror(errno));
		if (checkmod("build", "build.c")) {
			if ((pid = fork()) == 0)
				exec("/usr/bin/cc", (char *[]){"cc", "-o", "build", "build.c", NULL},
				     "Unable to execute compilation of `%sbuild.c'\n", path);
			pwait(pid, "Unable to delegate compilation of `%sbuild.c'\n", path);
		}
		exec("build", (char *[]){"build", NULL}, "Unable to execute `%sbuild'\n", path);
	}

	pwait(pid, "Unable to delegate execution of `%s'\n", path);
	printf("cd ..\n");
}
