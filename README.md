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

## Building/Running the JIT

Proton has a built-in JIT Compiler, you can build this compiler using the following commands

```console
$ git clone https://github.com/blanketsucks/language.git
$ cd language
$ make jit
```

After the build, you'll have a `proton-jit` executable in the same directory.

```console
$ ./proton-jit hello.pr
Hello, World!
```
