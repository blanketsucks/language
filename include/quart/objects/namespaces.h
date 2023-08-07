#pragma once

#include <quart/lexer/location.h>

#include <string>

struct Scope;

struct Namespace {
    std::string name;
    std::string qualified_name;

    Scope* scope;

    Span span;

    Namespace(const std::string& name, const std::string& qualified_name) : name(name), qualified_name(qualified_name) {};
};
