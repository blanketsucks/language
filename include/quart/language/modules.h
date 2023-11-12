#pragma once

#include <quart/filesystem.h>

#include <memory>

namespace quart {

struct Scope;

struct Module {
    std::string name;
    fs::Path path;

    bool is_ready;
    bool is_standard_library; // If the module is part of the standard library

    Scope* scope;

    Module(const std::string& name, const fs::Path& path, Scope* scope = nullptr);

    std::string to_string(char sep = '.');
};

}