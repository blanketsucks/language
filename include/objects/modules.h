#ifndef _OBJECTS_MODULES_H
#define _OBJECTS_MODULES_H

#include "utils/filesystem.h"

struct Scope;

struct Module {
    std::string name;
    std::string qualified_name;

    utils::filesystem::Path path;

    bool is_ready;

    Scope* scope;

    Module(
        std::string name, std::string qualified_name, utils::filesystem::Path path
    ) : name(name), qualified_name(qualified_name), path(path), is_ready(false) {};
};

#endif
