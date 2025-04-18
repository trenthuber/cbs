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

The advantage of making a library that only builds C projects is that it can be made dead simple. The build system itself just needs a way of executing build files recursively, and each build file just needs to compile and link C code. cbs uses three simple functions accordingly: `build()` for recursion, `compile()` for compiling, and `load()` for linking.

### Recurring build files

```c
void build(char *path);
```

It is often advantageous to compartmentalize projects into a number of subdirectories both for organizational purposes and for rebuilding parts at a time without needing to rebuild the whole thing. The usual way this is done is by placing build scripts in any subdirectory you want to rebuild on its own. These scripts are both able to be ran by the programmer from a shell as well as being able to be run by the build system itself from some parent directory. In cbs, it is the `build()` function that performs this function.

Build files must be named `build.c`, and the resulting build executable will always be named `build`. Each build file is in charge of building the contents of its resident directory. `build()` gets passed the name of the subdirectory you want to build, either as an absolute or relative path. It should be noted that **all relative paths in a build file are always assumed to be with respect to the directory that that build file is in**, not the root directory of the project. If `path` is a null pointer, this has the effect of staying in the current directory and thus (if the build file was modified) recompiling its own build file and rerunning itself. In essence, this function is what allows cbs to be truly automated.

An example of building the contents of the directories `abc/src/` and `/usr/local/proj/src/`.

```c
build("abc/src/");
build("/usr/local/proj/src/");
```

### Compiling source files

```c
void compile(char *src, ...);
```

The `compile()` function is given a single source file to compile. It will only run if it finds the source file has been modified since the last time it compiled it. This is similar to the caching behavior in most other build systems. When cbs generates an object file, it will be of the same name as its source file and in the same directory. The philosophy cbs takes on file extensions is to only use them when using *non-typical* file extensions, as **typical file extensions are always implicit**. What counts as a "typical" file extension depends on the situation. In the case of compiling, source files have the typical extension `.c` and object files have `.o`. As it turns out dropping the typical file extensions has its advantages in writing concise code.[^1]

[^1]: One example is putting a list of source file names without the `.c` extension in a C macro. We can use it to initialize an array that we iterate through, passing its elements as arguments to `compile()`, but we can also pass the entire macro as arguments to `load()` to link all the object files we just generated, ensuring we don't ever miss one.

If the source files you're compiling use project header files themselves and you wish to trigger recompilation should the header files be modified, you can pass them as additional arguments after the source file. The typical file extension assumed for these arguments is `.h`.

In all cases, whether or not you pass additional arguments, **the arguments passed to `compile()` must be terminated with a null pointer**.

To set flags to be passed to the compiler, the predefined `cflags` variable is used by setting it equal to an array of C strings which is also terminated with a null pointer. Unless reinitialized, the same flags will be used in all subsequent `compile()` calls. If no flags are needed, `cflags` can be set to a null pointer.

An example of compiling `main.c` which depends on `foo.h` and `bar.h`. This function call will produce the file `main.o`.

```c
cflags = (char *[]){"-Wall", "-O3", NULL};
compile("main", "foo", "bar", NULL);
```

### Linking object files

```c
void load(char type, char *target, char *obj, ...);
```

The first argument to `load()`[^2] is the type of the target file. The types are as follows.

[^2]: Although the term "linking" is far more common to use nowadays, the original term when UNIX was first created was "loading," so I use it here to name the function that does the linking. Also the name "link" is already taken on UNIX based systems.

```
'x' - executable
's' - statically linked library
'd' - dynamically linked library
```

The second argument is the file name of the target. There is no assumed typical file extension for the target as executables commonly lack them. It is also common to prepend `lib` to files that are static or dynamic libraries; this is done automatically by `load()` if necessary. The idea is to replicate the manner in which you would typically pass system libraries to the linker flag `-l`.

The rest of the arguments are the names of the files you want to link together. The typical file extension for these files is `.o` since we generally link object files. The only other files that would use a different file extension would be statically linked libraries or dynamically linked libraries (the linker flag `-l` should be used to link system libraries as opposed to project libraries). The `DYEXT` macro has been defined which represents the platform-dependent file extension for dynamic libraries: `".dylib"` for macOS and `".so"` for anything else. This helps ease the portability of build files.

As is the case with `compile()`, **the arguments passed to `load()` must be terminated with a null pointer**.

The predefined `lflags` variable is used to pass flags to the linker, used in a manner similar to the way `cflags` is used for compiling.

An example of linking `liba.so` (or `liba.dylib` on macOS), `b.o`, and `libc.a`, as well as the system math library into the statically linked library `libmain.a`.

```c
lflags = (char *[]){"-lm", NULL};
load('s', "main", "a" DYEXT, "b", "c.a", NULL);
```
