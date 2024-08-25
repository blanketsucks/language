#pragma once

#include <quart/common.h>
#include <quart/errors.h>
#include <quart/compiler.h>

#include <set>

namespace quart::cl {

struct Arguments {
    fs::Path file;

    String output;
    String entry;
    String target;

    Vector<String> imports;
    
    std::set<String> library_names;
    std::set<String> library_paths;

    OutputFormat format;
    MangleStyle mangle_style;
    
    bool optimize = false;
    bool verbose = false;
    bool no_libc = false;
    bool print_all_targets = false;

    bool jit = false;
};

ErrorOr<Arguments> parse_arguments(int argc, char** argv);

}