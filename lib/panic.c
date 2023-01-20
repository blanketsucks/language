#include <stdio.h>
#include <stdlib.h>

#define __USE_GNU
#include <pthread.h>

__attribute__((noreturn)) void __quart_panic(
    const char* file,
    int line,
    int column,
    const char* msg
) {
    char name[16];
    pthread_getname_np(pthread_self(), name, sizeof(name));

    fprintf(stderr, "%s:%d:%d: Panic in thread '%s': %s\n", file, line, column, name, msg);
    abort();
}