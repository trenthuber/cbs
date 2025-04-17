# cbs

Many modern programming languages integrate some form of a build system into their own runtimes, allowing developers to use the same language to write their applications and to build them. This library hopes to bring that functionality to the C language.

cbs is a build system for C projects and is itself written in C.

## Overview

Build "scripts" are written in files called `build.c`. Here is a minimal example of the contents of such a file.

```c
#include "cbs.c"

int main(void) {
	build(NULL);

	compile("main", NULL);
	load('x', "main", "main", NULL);

	return 0;
}
```

To build your project, you first need to manually compile `build.c` and run the resulting executable, called `build`.

```console
$ cc -o build build.c
$ ./build
cc -c -o main.o main.c                                                                                                                   
cc -o main main.o
$ ./main
Hello, world!
```

The inclusion of the `build(NULL);` statement at the top of the build file means that when you modify the build file after compiling the build executable, you don't need to recompile the build file again as the old executable will recompile it for you and rerun the new build executable automatically. This means **you only need to manually compile `build.c` once, even if you modify it later**.

```console
$ touch main.c build.c
$ ./build
cc -o build build.c
./build
cc -c -o main.o main.c
cc -o main main.o
```

## Detailed usage

The advantage of making a build system that only compiles C projects is that it can be made dead simple. C code just needs to be compiled and linked, and perhaps it would be nice to recurse the build into subdirectories. cbs uses three simple functions accordingly: `compile()` for compiling, `load()` for linking, and `build()` for recursion.

### Compiling

```c
void compile(char *src, ...);
```

The `compile()` function is given a single source file to compile. cbs will generate an object file of the same name as the source file and in the same directory. The general philosophy cbs takes on file extensions is to only use them when using *non-typical* file extensions, as **typical file extensions are always implicit**. In the case of compiling, source files have the typical extension `.c` and object files have `.o`. This has the convenient side effect of letting us reuse the list of source file names as object file names for the linker.[^1]

[^1]: One example is putting a list of comma-separated C strings in a macro. Then we can pass it both to function calls as arguments and to array initialization as elements. Using the macro in a null-terminated array allows us to iterate through the array and use them as the names of source files to compile, while using the macro in a function call allows us to link all the object files generated by the compilation.

`compile()` will only run if it finds the source file has been modified since the last time it compiled the resulting object file. This is similar to the caching behavior in most other build systems.

If the source file uses project header files and you wish to trigger recompilation should they be modified, you can add their names after the name of the source file. The typical file extension assumed for these arguments is `.h`. In all cases, whether or not these additional arguments are included, **the arguments passed to `compile()` must be terminated with a null pointer**.

To set flags to be passed to the compiler, the predefined `cflags` variable is used by setting it equal to an array of C strings which is terminated with a null pointer. Unless reinitialized, the same flags will be used in all subsequent `compile()` calls. `cflags` can be set to a null pointer when no flags are needed.

An example of compiling `main.c` which depends on `foo.h` and `bar.h`. This function call will produce the file `main.o`.

```c
cflags = (char *[]){"-Wall", "-O3", NULL};
compile("main", "foo", "bar", NULL);
```

### Linking

```c
void load(char type, char *target, char *obj, ...);
```

The first argument to `load()`[^2] is the type of the target file. The options are as follows.

[^2]: Although the term "linking" is far more common to use nowadays, the original term when UNIX was first created was "loading," so I use it here to name the function that does the linking. Also the name "link" is already taken on UNIX based systems.

```
'x' - executable
's' - statically linked library
'd' - dynamically linked library
```

The second argument is the name of the target. There is no assumed typical file extension for the target as executables commonly lack them. It is also common to prepend `lib` to files that are static or dynamic libraries; this is done automatically and in a nature similar to typical file extensions. The idea is similar to the manner in which you would typically pass system libraries to the linker flag `-l`.

The rest of the arguments are the names of the files you want to link together. The typical file extension for these files is `.o`. Generally the only other files that would use a different file extension would be statically linked libraries or dynamically linked libraries (the linker flag `-l` should be used to link system libraries as opposed to project libraries). As is the case with compiling, **the arguments passed to `load()` must be terminated with a null pointer**.

In a similar way as compiling, the predefined `lflags` variable can be used to define flags for the linker.

The `DYEXT` macro has been defined which represents the platform-dependent file extension for dynamic libraries, `".dylib"` for macOS and `".so"` for anything else. This can be used to allow portability of build files.

An example of linking `liba.dylib` (or `liba.so`), `b.o`, `libc.a`, and the system math library into a statically linked library `libmain.a`.

```c
lflags = (char *[]){"-lm", NULL};
load('s', "main", "a" DYEXT, "b", "c.a", NULL);
```

### Recursive builds

```c
void build(char *path);
```

It is often advantageous to compartmentalize projects into a number of subdirectories both for organizational purposes and for rebuilding parts at a time without needing to rebuild the whole thing. The usual way this is done is by placing build scripts in any subdirectory you want to rebuild on its own. These scripts double as both being able to be ran by the programmer from the shell as well as being able to be run by the build system itself from some parent directory. The `build()` function performs the latter function of the build system.

`build()` gets passed the name of the subdirectory you want to build, either as an absolute or relative path. Another philosophy taken by cbs is that **relative paths in a build file are always assumed to be with respect to the directory that that build file is in**. The directory you pass `build()` must have a `build.c` file in it which will be (re)compiled and (re)run. If `path` is a null pointer, this has the effect of staying in the current directory and (again, if necessary) recompiling the build file and rerunning the build executable therein. Putting `build(NULL);` at the start of build files is what allows a build executable to recompile itself in the event its corresponding build file gets changed. In essence, this function is what allows cbs to be truly automated.

An example of building the contents of the directories `abc/src/` and `/usr/local/proj/src/`.

```c
build("abc/src/");
build("/usr/local/proj/src/");
```
