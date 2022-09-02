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
=

You can run these examples like the following

```console
$ ./proton hello.pr
$ ./hello
Hello, world!
```

## Building

Make sure to have a C++ compiler (such as `g++`) and LLVM installed. LLVM 14 or higher is required.

```console
$ git clone https://github.com/blanketsucks/language.git
$ cd language
$ make 
```

## Goals

- [x] ~~For/while loops. The parsing for while loops is done, but something goes wrong in the code generation.~~ Done.
- [x] ~~return inside if statements. Already works if we make LLVM shut up.~~ Solved.
- [ ] More preprocessor directives.
- [ ] Better errors.
- [ ] Rewrite include system and maybe implement modules?
- [ ] Fix memory leaks.
- [ ] Name mangling.
- [ ] Self-hosted.
- [ ] Generics.
