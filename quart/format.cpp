#include <quart/format.h>

namespace quart {

void outln(const char* str) {
    std::cout << str << '\n';
}

void outln(const String& str) {
    std::cout << str << '\n';
}

void outln() {
    std::cout << '\n';
}

}