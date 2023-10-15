#pragma once

#include <quart/common.h>

#include <stdint.h>
#include <string>
#include <sstream>

#include <llvm/ADT/StringRef.h>

namespace quart {

struct Location {
    u32 line;
    u32 column;
    size_t index;
};

struct Span {
    Location start;
    Location end;

    llvm::StringRef filename;
    llvm::StringRef line;

    Span() = default;
    Span(Location start, Location end, llvm::StringRef filename, llvm::StringRef line);

    static Span merge(const Span& start, const Span& end);

    size_t length() const;
};

}
