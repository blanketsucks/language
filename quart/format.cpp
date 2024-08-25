#include <quart/format.h>

namespace quart {

void outln(const char* str) {
    llvm::outs() << str << '\n';
}

void outln() {
    llvm::outs() << '\n';
}

}