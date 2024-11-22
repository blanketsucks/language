# Language

Small language made for fun.

## Building

Make sure to have `cmake` installed, a C++ compiler (such as `g++`) and LLVM 17 installed (other versions are not supported).

```console
$ git clone https://github.com/blanketsucks/language.git
$ cd language
$ mkdir build
$ cd build
$ cmake ..
$ make
```

By the end of the build process, a `quart` executable will be created in the `build` directory.

```console
$ ./build/quart --help
USAGE: quart [options] <files>

OPTIONS:

Compiler options:

  -I=<path>              - Add an include path
  --entry=<string>       - Set an entry point for the program
  --format=<value>       - Set the output format
    =llvm-ir             -   Emit LLVM IR
    =llvm-bc             -   Emit LLVM Bitcode
    =asm                 -   Emit assembly code
    =obj                 -   Emit object code
    =exe                 -   Emit an executable (default)
    =shared              -   Emit a shared library
  -l=<name>              - Add a library
  --mangle-style=<value> - Set the mangling style
    =full                -   Use the default mangling style
    =minimal             -   Use a minimal mangling style
    =none                -   Do not mangle names
  --optimize             - Enable optimizations
  --output=<string>      - Set an output file
  --verbose              - Enable verbose output

Generic Options:

  --help                 - Display available options (--help-hidden for more)
  --help-list            - Display list of available options (--help-list-hidden for more)
  --version              - Display the version of this program
```