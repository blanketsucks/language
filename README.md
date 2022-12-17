# Language

Small language made for fun.

```
import libc;

func main() {
    libc::printf("Hello, world!\n");
}
```

And then:

```console
$ ./quart hello.qr
$ ./hello
Hello, world!
```

## Building

Make sure to have a C++ compiler (such as `g++`) and LLVM installed. LLVM 14 (Other versions of LLVM are currently not supported) is required.

```console
$ git clone https://github.com/blanketsucks/language.git
$ cd language
$ make 
```

By the end of the build process, you'll have a `quart` executable in the same directory.

```console
$ ./quart --help
USAGE: quart [options] <files>

OPTIONS:

Compiler options:

  -I=<path>         - Add an include path
  --entry=<string>  - Set an entry point for the program
  --format=<value>  - Set the output format
    =llvm-ir        -   Emit LLVM IR
    =llvm-bc        -   Emit LLVM Bitcode
    =assembly       -   Emit assembly code
    =object         -   Emit object code
    =exe            -   Emit an executable (default)
    =shared         -   Emit a shared library
  -l=<name>         - Add a library
  --optimize        - Enable optimizations
  --output=<string> - Set an output file
  --verbose         - Enable verbose output

Generic Options:

  --help            - Display available options (--help-hidden for more)
  --help-list       - Display list of available options (--help-list-hidden for more)
  --version         - Display the version of this program
```

## Building/Running the JIT

Quart has a built-in JIT Compiler, you can build this compiler using the following commands

```console
$ git clone https://github.com/blanketsucks/language.git
$ cd language
$ make jit # or make all which will build both the JIT and the compiler
```

After the build, you'll have a `quart-jit` executable in the same directory.

```console
$ ./quart-jit hello.qr
Hello, World!
```
