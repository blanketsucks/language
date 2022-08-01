# Language

Small language made for fun.

## Example

```
extern "C" func printf(fmt: str, ...) -> int;

func main() {
    printf("Hello, world!\n");
}
```
Or:
```
$include "libc/include.pr"

func main() {
    libc::printf("Hello, world!\n");
}
```
Or using the incomplete standard library:
```
$include "std/io.pr"

func main() {
    let stdout = std::io::stdout();
    stdout.write("Hello, world!\n");
}
```

You can run these examples like the following

```console
$ ./proton hello.pr
$ ./hello
Hello, world!
Segmentation fault
``` 
I have no idea why it segfaults, but the following doesn't

```console
$ ./proton hello.pr -c
$ gcc hello.o -o hello
$ ./hello
Hello, world!
```

The `-c` flag compiles the code down to an object file.

## Building

Make sure to have a C++ compiler (such as `g++`) and LLVM installed. LLVM 14 or higher is required.

```console
$ git clone https://github.com/blanketsucks/language.git
$ cd language
$ make 
```

## Goals

    - [ ] For/while loops. The parsing for while loops is done, but something goes wrong in the code generation.
    - [ ] return inside if statements. Already works if we make LLVM shut up.
    - [ ] More preprocessor directives.
    - [ ] Better errors.
    - [ ] Fix memory leaks.
    - [ ] Name mangling.
    - [ ] Self-hosted.
    - [ ] Generics.
