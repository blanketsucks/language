#pragma once

#include <quart/filesystem.h>
#include <quart/compiler.h>

namespace quart {

namespace cl {

struct Arguments {
    fs::Path file;
    std::string output;
    std::string entry;
    std::string target;

    std::vector<std::string> imports; // vector of import paths
    Libraries libraries;

    OutputFormat format;
    MangleStyle mangle_style;
    
    bool optimize = false;
    bool verbose = false;
    bool standalone = false;
    bool print_all_targets = false;

    bool jit = false;
};

Arguments parse_arguments(int argc, char** argv);

}

}