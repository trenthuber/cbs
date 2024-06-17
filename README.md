# cbs

## Summary
cbs is a build system for C written in C (although it can certainly be used to build more than just C projects). While most modern day build systems are far more feature rich than this project, they lack at least one thing: a fully developed and mature build language.

That's where cbs comes in.

In essence, build scripts written with cbs are only constrained by the capabilities of the C programming language itself, not some superficially created one. While perhaps more verbose than others, C's capabilites as a "real" and fully-fleshed out programming langauge make it worth the effort in the end. To make the process that much easier, much of the functionality that is common in building projects has been wrapped up in this single-file library, including running Bourne shell commands, navigating file trees, extracting names and extensions of files, and even multi-process building (similar to the -j option in Make).

## Getting Started
To use the library, simply create a `cbs.c` file and include `cbs.h`. Similar to other single-file libraries, you can include the actual implementation by defining the `CBS_IMPLEMENTATION` macro. Common boilerplate for cbs looks like:
```c
#define CBS_IMPLEMENTATION
#include "cbs.h"

int main(int argc, char **argv) {
	cbs_rebuild_self(argv);
	cbs_shift_args(&argc, &argv);

	// Build script goes here

	return 0;
}
```

To run your build program initially, you must first bootstrap it. Simply run:
```console
$ cc -o cbs cbs.c && ./cbs # Run this the first time
```

Every time after that, the `cbs_rebuild_self()` function (assuming you put it at the top of your program) will handle rebuilding the `cbs.c` program and rerunning for you. Thus, to build your project after the initial bootstrapping, you only need to run:
```console
$ ./cbs # Run this every time after
```

See `cbs.h` for an overview on the interface.
