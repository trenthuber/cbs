# cbs

Many modern programming languages integrate some form of a build system into their own runtimes, allowing developers to use the same language to write their applications and to build them. This library hopes to bring that functionality to the C language.

cbs is a build system for C projects and is itself written in C. This gives the added bonus of only needing a C compiler to build and compile projects.

## Overview

As opposed to making a build script, you instead make a file called `build.c` which includes `cbs.c`. A simple example is seen below:

```c
#define CFLAGS ""
#define LDFLAGS ""

#include "cbs.c"

int main(void) {
	build(NULL);

	CC("main");
	LD('x', "main", "main");

	return 0;
}
```

Next, you compile the build file into an executable. You only need to do this once if you include the `build(NULL);` statement at the top of the build file. In that case every subsequent time you call `./build` it will recompile and rerun if necessary.

```console
% cc -o build build.c
```

Running the resulting `./build` executable will then build your project as described in the build file. Then you can run whatever was meant to be built, in this case a hello world program:

```console
% ./build
% ./main
Hello, world!
```

## Usage

The advantage of only supporting C projects is that there are really only three things that need to be covered: compiling translation units to object files, linking object files together, and recursing the build into subdirectories.

### Preprocessing: `CFLAGS` and `LDFLAGS`

Before you include `cbs.c` it is important you define the `CFLAGS` and `LDFLAGS` macros. Their use should be obvious. If you have no use for one or both of the flags, then they must be left as empty strings:

```c
#define CFLAGS ""
#define LDFLAGS ""
```

These are macros used by the library and it expects them to exist. Any flags you want to pass to the compiler or linker must be written as a comma separated list of C strings with no trailing comma, for example:

```c
#define CFLAGS "-Ifoo/", "-Ibar/", "-Dbaz"
#define LDFLAGS "-lm", "-Llib/"
```

If you ever want to convert a comma separated list of strings to an array of C strings, the simple `CARRAY` macro does so.

The `CFLAGS` and `LDFLAGS` macros will automatically be used in all compilations and linkages performed by the build file they're defined in.

### Compiling: `cc()`

```c
void cc(char *name, ...);
```

The `cc()` function is given a single source file to compile. Extensions are optional and will be assumed to be `.c` if not provided. This makes reusing the same macro for compiling and linking very elegant ([seen here](https://github.com/trenthuber/simplexpm/blob/f8fbfccbdc4e966c0565e86541fd6a9e6b92ac55/build.c#L38)). It will then produce its corresponding `.o` file in the same directory as the source file.

`cc()` will only compile the object file if it finds the source file has been modified since the last time it compiled the object file. If the object file also depends on other header files that you wish to trigger recompilation should they change, you can add their names after the name of the source file (again, the `.h` is optional and assumed if missing).

In all cases, `cc()` requires a null pointer to terminate the arguments (one common convention for implementing C variatics). If you find the syntax unsightly (as I do) or often forget them (as I do), there's a simple `CC` macro which calls `cc()` with a null pointer automatically added.

An example of compiling `main.c` which depends on `foo.h` and `bar.h` and produces the file `main.o`:

```c
CC("main", "foo", "bar");
```

### Linking: `ld()`

```c
void ld(char type, char *output, char *input, ...);
```

The first argument to `ld()` is the type of file to link. The options are:

```
'x' - executable
's' - statically linked library
'd' - dynamically linked library
```

The second argument is the name of the output. As mentioned above, the system will automatically add the correct file extension if not provided depending on `type`. It is also common to prepend `lib` to files that are static or dynamic libraries; this is similarly optional and will be automatically prepended if needed.

The rest of the arguments are the names of the files you want to link together. It is assumed that any file that doesn't have an extension is an object file, so if passing static or dynamic libraries, make sure to include the extensions. It should be noted that you can often reuse the list of names for source files as object files without any modification since they normally correspond one to one, have the same name, and cbs very conveniently doesn't require extensions in file names where it can be assumed.

Similar to `cc()` and `CC`, the `ld()` function expects a null pointer as the last argument which the `LD` macro provides automatically.

An example of linking `a.o`, `lib/libb.dylib` (sorry, macOS), and `c.o` into a static library, `libmain.a`:

```c
LD('s', "main", "a", "lib/b.dylib", "c");
```

### Recursive builds: `build()`

It is often adventagous to compartmentalize projects into a number of subdirectories, either to keep dependencies organized or to have the final build components be independent of each other until it's time to link. Additionally, while working on the code in one subdirectory, it is rare we'd need to recompile the entire project. It would be nice to have a build system that allows us to compile things part by part or all at once without redundancy.

The usual way this is done is by having build scripts be able to run other build scripts. Thus, you can make a build script for a subdirectory that does everything it needs to do, and then have the build script in the directory above call that script when it needs to compile that subdirectory. The elegance to this approach is that you can reuse the build scripts from all your subdirectories in the build process for the whole project.

```c
void build(char *path);
```

cbs has a similar feature and it's used via the `build()` function which gets passed a single argument: the path to the subdirectory you want to build. When called, it will switch to the directory corresponding to the path, run the `build` file found there (recompiling `build.c` if necessary), and change back to the directory it came from. Thus, when making a build script in a subdirectory you can write it with all paths relative to that directory and it will translate even if you build it from another directory with a call to `build()`.

This concept can actually be reused to have a build executable rebuild itself if its own `build.c` file is modified. By passing `NULL` as the argument, `build()` will recompile and rerun the `build.c` file in it's own directory. Putting `build(NULL);` at the top `main()` in every build file means the programmer only needs to manually compile the top level build file once. Every other time in every other subdirectory rebuilding is covered when running `./build`.

An example of building the contents of the folder `abc/src/`:

```c
build("abc/src/");
```
