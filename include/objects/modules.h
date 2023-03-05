#ifndef _OBJECTS_MODULES_H
#define _OBJECTS_MODULES_H

#include "utils/filesystem.h"
#include "utils/string.h"

struct Scope;

struct Module {
    std::string name;
    utils::fs::Path path;

    bool is_ready;
    bool is_stdlib;

    Scope* scope;

    Module(
        std::string name, utils::fs::Path path
    ) : name(name), path(path), is_ready(false) {
        auto parts = this->path.parts();
        this->is_stdlib = parts[0] == "lib";
    };

    std::string get_clean_path_name(bool replace_with_dots = false) {
        std::string clean = this->path.str();
        if (this->is_stdlib) {
            clean = clean.substr(4);
        }

        if (this->path.isfile()) {
            clean = clean.substr(0, clean.size() - 3);
        }

        return replace_with_dots ? utils::replace(clean, "/", ".") : clean;
    }
};

#endif
