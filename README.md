# cbs

cbs is an extremely lightweight build system designed specifically for C projects. 

## Overview

To build a project, you first need to make a file called `build.c` which describes what to build.

```c
// build.c

#include "cbs.c"

int main(void) {
    build("./");

    cflags = NONE;
    compile("main");

    lflags = NONE;
    load('x', "main", LIST("main"));

    return EXIT_SUCCESS;
}
```

Next, compile the build file and run the resulting executable, called `build`.

```console
> cc -o build build.c
> build
cc -c -o main.o main.c
cc -o main main.o
> main
Hello, world!
```

Every subsequent time you run `build`, it will rebuild the entire project, including itself.

```console
> build
cc -c -o build.o build.c                                                                                                                            
cc -o build build.o
cc -c -o main.o main.c
cc -o main main.o
```

## Detailed usage
cbs tries to be as simple as possible, while still remaining powerful. Its simplicity is rooted in its intentionally limited scope of only building C projects. Thus, cbs only needs to compile and link C code, as well as call other build executables.

### Compiling source files

```c
void compile(char *src);
```

The `compile()` function is given a single source file to compile and will generate an object file with the same name. In general, file extensions are unnecessary in your build files as they can usually be inferred based on the function being called. This has the added benefit of being able to reuse lists of file names in both compiling and linking.

Before you run `compile()`, the global `cflags` variable has to be initialized with a list of flags to pass to the compiler. If no flags are needed, than the `NONE` macro should be used; otherwise the `LIST()` macro can be used.

```c
cflags = LIST("-Wall", "-O3");
compile("main");
```

> [!NOTE]
> It is not guaranteed that the object code cbs produces will be position-independent. When compiling source files that will be used in a dynamic library, you will need to include `-fPIC` in your compiler flags to ensure compatibility between platforms.

### Linking object files

```c
void load(char type, char *target, char **objs);
```

The first argument tells `load()` the type of target file to generate.

```
'x' - executable
's' - static library
'd' - dynamic library
```

The second argument is the name of the target file. To aid portability, the file extension is optional, as it can be inferred from the target file type. It is also common to prepend `lib` to libraries; this is similarly optional.

The third and final argument is a list of object files and libraries that will be linked to create the target file. Here, file extensions *are* required for libraries since they're mixed in with object files. The `LIST()` macro can also be used for this list since it too is expected to be NULL-terminated.

Similar to `cflags`, there is a global `lflags` variable used by the linker.

```c
lflags = LIST("-lm");
load('s', "main", LIST("first" DYEXT, "second", "third.a"));
```

`DYEXT` is macro defined to be the platform-specific file extension for dynamic libraries, to aid the portability of build files.

### Recursive builds

```c
void build(char *path);
```

The `build()` function allows one build executable to run another build executable. The name of the directory that contains the build executable being run is passed to the function by a relative or absolute path. You can think of this function as changing to that directory and running the build executable located therein. `build()` will only recompile the build file it if it can't find the build executable in that directory.

If the current directory is passed to `build()`, then it will *recompile its own build file* before rerunning *itself*. Thus, including a statement like `build("./");` at the beginning of your build file means you don't have to manually recompile that build file every time you modify it.

```c
build("./");
build("../../src/");
build("/usr/local/project/src/");
```
