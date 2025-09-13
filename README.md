# Edgerunner

This is a collection of renderers built for the sole aim of learning graphics programming. 
These are mostly contained in a single file and do not represent how I think a proper production system should be architected, but in this case where I am just trying to learn a particular concept, its really convenient and posess the least resistance to progress!

In the main part of this directory, there are multiple folders that represent different 3D APIs. Each folder contains a complete build script that allows you to create a reproduceable build on your computer as long as you follow the requirements in the [Prereqs](#Prereqs) section.

Note that there are different renderers that have different features but when you build (i.e. building a basic renderer with a triangle is as simple as `build hello debug`) it replaces the previously built executable. The executable is available in the `run_tree` directory, called `edgerunner.exe`, and associated files.

The `10x` editor is used, and a configuration file is provided. If you use it, just double click the `edgerunner.10x` file and it should work when you hit the build key (whatever you have that mapped to!).

The `ctime.exe` is used to track compile times on your local machine and see just how fast your compiles are. If would like to diable it, you can comment out the appropriate line in the `build.bat` script, nothing bad would happen when building, you just wont any idea how long a compile took. An executable is provided in the `miscl` folder. It was created by Casey Muratori.

## Prereqs

>[!NOTE]
> Only x64 Windows 10 and above is supported.

1. Install Required Tools (Windows SDK & MSVC)
You need the [Microsoft C/C++ Build Tools](https://visualstudio.microsoft.com/downloads/?q=build+tools) 
for both the Windows SDK and the MSVC Compiler. 
Alternativly, Clang can also be used to build the executable, but you would still need the Windows SDK. 
[Clang](https://releases.llvm.org/)

2. Build Environment
The renderer can be using MSVC or Clang in the command line. This is done by calling 
vcvarsall.bat x64 (included with the Microsoft C/C++ Build Tools). This can be done automatically by 
the `x64 Native Tools Command Prompt for VS <year>` cmd variant installed by the Microsoft C/C++ Build Tools. 
If you have installed the build tools, the command prompt can be located by searching for 
*native* from the Windows Start Menu.

To confirm that you have access to the MSVC Compiler after opening the cmd variant, run:

```
cl
```

If everything is set up right, you should see output similar to this:
```
Microsoft (R) C/C++ Optimizing Compiler Version 19.42.34435 for x64
Copyright (C) Microsoft Corporation.  All rights reserved.

usage: cl [ option... ] filename... [ /link linkoption... ]
```

3. Building
Within the `x64 Native Tool Command Prompt`, `cd` to the root directory of the codebase and run the 
`build` script like so:

```
## For MSVC
build hello msvc debug

## For Clang
build hello clang debug
```

You should see the following: 
```
[release mode]
[compiling with msvc]
main.cc
```
