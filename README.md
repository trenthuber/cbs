# cbs, C Build System

## Motivation
The idea for this project is directly stolen from a recreational programming streamer, [Tsoding Daily](https://www.youtube.com/watch?v=eRt7vhosgKE&list=PLpM-Dvs8t0Va1sCJpPFjs2lKzv8bw3Lpw), specifically from his [musializer](https://github.com/tsoding/musializer) project.

**As it stands:** Practically speaking, building a complex C project requires a build system to build it. These build systems, such as Make or CMake, all depend on other pieces of software you need preinstalled to your computer before you can even start the build process. But why should a C project depend on anything besides a C compiler? I mean, for as "low-level" as we call C, shouldn't we be able to do everything we desire with only a C compiler?

**As it *should* stand:** A C project should be able to build using only a C compiler. This means replacing any previously used build systems with a system that is essentially just a C library and replacing our precious build scripts with C source code that uses that C library. In this way, we not only ditch any build dependency, but we also have the luxury (some might think it far from it) of being able to write our build script in the same language we wrote the rest of our project in! No need to learn how to write a Makefile or a CMakeList.txt, we can write it all in one lanaguage.

This idea can technically be applied (and perhaps applied easier than it is here) to any other language, but it seems fitting for it to at least first be tried in C given how low-level C projects already are.

## Obstacles
In order for this library to actually solve our problem, we can't have it be something you need to install in order to use. So, it's necessary that we go with a [single-file library](https://github.com/nothings/single_file_libs) structure for this build system project. All you need to do then is copy a *single file* into your project and create a build script in C. Additionally, since the build system is entirely contained inside the project, anyone that builds it for themselves only needs a C compiler. It's an inherently portable solution.

Using C source code as a build script means that we would need to compile the code before we run it. This is fine if we never plan on changing the build script but realistically, this would mean having to recompile the build script source code every time we wanted to build our project. This is suboptimal, but cleverly solved by having the previous build system executable *rebuild itself* every time the source code is updated (see [File caching](#file_caching)). This requires putting the `cbs_rebuild_self` function at the top of the build script.

## <a name="build"></a>Build
Make sure to copy the `cbs.h` file to project and in the **same directory** make a `cbs.c` file with the build script (an [example template](./cbs_template.c) has been included).

The **first time** you build your project, you'll need to [bootstrap](#bootstrap) the build system executable as such:

```console
$ cc -o cbs cbs.c
$ ./cbs
```

Every time after that you **don't need to bootstrap again**, and can just run

```console
$ ./cbs
```

cbs has functions that make parsing command line options easy. Using them, you could run your own defined subcommands. For example:

```console
$ ./cbs build
$ ./cbs run
$ ./cbs clean
```

## Features
### <a name="bootstrap"></a>Automatic build system rebuilding
As previously mentioned, you can include a function call at the top of the build script and it will make the build system recompile its own build script every time it runs. This means running your build system executable will handle everything for you, there is no need to manually recompile your build script every time.

#### Note 1:
When you first create you build script there is no initial build executable to automatically rebuild, so you must first *bootstrap* the executable by compiling the build script yourself (see [Build](#build)). This step is *only done once* as an initialization step. Once you've generated the executable, you shouldn't need to bootstrap again.

#### Note 2:
When using cbs, you must include a file named `cbs.c` in the same directory as you have `cbs.h` in, and the executable generated must also be originally bootstrapped in the same directory. These are necessary requirements for the rebuild feature to work properly. The generated build system executable will have the same name as the executable you ran to build it, so, for the sake of habit, you can name the originally bootstrapped executable anything you wish and that name will persist.

### <a name="file_caching"></a>File caching
Make looks at the modification time of the dependencies of each recipe and compares them to the modification time of the target. If any dependencies were modified after the target, it executes the recipe. It is literally as simple as it sounds. cbs can do the exact same thing with functions that check if targets need to be rebuilt. cbs uses this feature when rebuilding itself. 

### Multi-process execution
Larger projects can become quite slow to build, especially if multiple dependencies need to be recompiled inside the project. Many build systems offer the option of "multithreading" the build process, which really just means running different sections of the build script in separate child processes. cbs offers similar functionality except the user has *direct control* over which commands get run asynchronously and which don't.

### File path pattern matching
Often times you want to run the same set of commands on multiple files in your project. Make offers the ability to pattern match target names to dependency names and then use both names in the recipe to avoid redundancy. cbs has a similar functionality in which you can search in certain directories for file paths ending in a specifed file extension and get back a dynamic array of their file paths which you can then use in building your commands. In some ways, this is far more intuitive than the approach make takes, even if it's a little more verbose.

## License
See [LICENSE](./LICENSE) file for license rights and limitations.
