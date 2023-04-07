#ifndef _OBJECTS_NAMESPACES_H
#define _OBJECTS_NAMESPACES_H

#include <quart/lexer/location.h>

#include <string>

struct Scope;

struct Namespace {
    std::string name;
    std::string qualified_name;

    Scope* scope;

    Span span;

    Namespace(std::string name, std::string qualified_name) : name(name), qualified_name(qualified_name) {};
};

#endif