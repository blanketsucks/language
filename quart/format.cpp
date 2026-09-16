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

String escape(const String& in) {
    String out;
    out.reserve(in.size()); // We reserve at least the size of the input string at first

    for (auto& c : in) {
        if (std::isprint(c)) {
            out.push_back(c);
            continue;
        }

        out.push_back('\\');
        switch (c) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '\n': out.push_back('n'); break;
            case '\r': out.push_back('r'); break;
            case '\t': out.push_back('t'); break;
            default:
                // FIXME: Handle hex case
                out.push_back(c);
        }
    }

    return out;
}

}