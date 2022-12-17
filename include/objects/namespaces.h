#ifndef _OBJECTS_NAMESPACES_H
#define _OBJECTS_NAMESPACES_H

#include "lexer/tokens.h"

#include <string>

struct Scope;

struct Namespace {
    std::string name;
    std::string qualified_name;

    Scope* scope;

    Location start;
    Location end;

    Namespace(std::string name, std::string qualified_name) : name(name), qualified_name(qualified_name) {};
};

#endif