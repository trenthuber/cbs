# cbs

Many modern programming languages integrate some form of a build system into their own runtimes, allowing developers to use the same language to write their applications and to build them. This library hopes to bring that functionality to the C language.

cbs is a build system for C projects and is itself written in C.

## Overview

Build files are always named `build.c`. Here is a minimal example of the contents of such a file.

```c
// build.c

#include "cbs.c"

int main(void) {
    build(NULL);

    compile("main");
    load('x', "main", (char *[]){"main", NULL});

    return 0;
}
```

To build your project, you first need to manually compile your build file and run the resulting executable, called `build`.

```console
$ cc -o build build.c
$ ./build
cc -c -o build.o build.c
cc -o build build.o
build
cc -c -o main.o main.c
cc -o main main.o
$ ./main
Hello, world!
```

The inclusion of the `build(NULL);` statement at the top of the build file will cause the build executable to *recompile its own source code* anytime that source code gets modified. This means **you only need to manually compile `build.c` once, even if you later modify it**.

```console
$ ./build
cc -c -o main.o main.c
cc -o main main.o
$ touch build.c
$ ./build
cc -c -o build.o build.c
cc -o build build.o
build
cc -c -o main.o main.c
cc -o main main.o
```

Note too that cbs rebuilt `main.c` even though it wasn't modified. This is because **cbs is not an incremental build system**. The bugs these kinds of systems produce can often waste more time for the programmer than they would otherwise save. Besides, cbs is intended for smaller projects where incremental builds really wouldn't save that much time anyway.

## Detailed usage
The advantage of cbs is its simplicity, which in turn comes from its intentionally limited scope of building C projects. cbs just needs a way of compiling and linking C code and perhaps a way of recursing into project subdirectories where the build process can be repeated.

### Compiling source files

```c
void compile(char *src);
```

The `compile()` function is given a single source file to compile. The object file it generates will have the same base name and will be in the same directory as the source file. In general, file extensions are unnecessary for cbs as they can usually be inferred based on the function being called. This has the added benefit that often data structures containing build file names can be reused for compilation and linking steps.

To pass flags to the compiler, the predefined `cflags` variable is set equal to a NULL-terminated array of C strings, each string being a compiler flag. Until reinitialized, the same flags will be used in all subsequent `compile()` calls. Setting `cflags` to NULL will prevent any flags from being used, which is its default value.

An example of using `cflags`:

```c
cflags = (char *[]){"-Wall", "-O3", NULL};
compile("main");
```

> [!NOTE]
> It is not guaranteed that the object code cbs produces will be position-independent. When compiling source files that will be used in a dynamic library, you will need to include the `-fPIC` flag in `cflags` to ensure compatibility between platforms.

### Linking object files

```c
void load(char type, char *target, char **objs);
```

The first argument to `load()` is the type of the target file. The types are as follows.

```
'x' - executable
's' - statically linked library
'd' - dynamically linked library
```

The second argument is the file name of the target. A file extension is optional as it'll be automatically appended if it's not present based on the target type. It is also common to prepend `lib` to libraries; this is done automatically as well in the case it's not present. This behavior is intended to replicate how you would typically specify system libraries to the linker flag `-l`.

The third and final argument is a NULL-terminated array of C strings, each string being the name of an object file or library to link together to create the target. File extensions for libraries are required here, as otherwise the system will assume it's the name of an object file.

The `lflags` variable works exactly like the `cflags` variable but with respect to the linker. An example using `lflags`:

```c
lflags = (char *[]){"-lm", NULL};
load('s', "main", (char *[]){"a" DYEXT, "b", "c.a", NULL});
```
The `DYEXT` macro is defined with the platform specific file extension for dynamic libraries to aid portability of build files.

### Building subdirectories

```c
void build(char *path);
```

It is often advantageous to compartmentalize projects into a number of subdirectories both for organizational purposes and for rebuilding parts at a time without needing to rebuild the entire project. The usual way this is done is by placing build scripts in any subdirectory you want to be able to rebuild on its own. These scripts can be run by the programmer from a shell *or* run by the build system itself from a higher-up directory. The latter functionality is performed by `build()`.

Every subdirectory you want to build should have its own build file in it which is responsible for producing the final product of that subdirectory. `build()` gets passed a C string which contains the name of the subdirectory to build, either as an absolute or relative path. It should be noted that **all relative paths in a build file are with respect to the directory that that build file itself is in**, not the root directory of the project. If `path` is NULL, this has the effect of staying in the current directory and thus recompiling its own build file and rerunning itself if the build file was modified since the last time it was compiled.

An example using `build()`:

```c
build("abc/src/");
build("/usr/local/proj/src/");
```
