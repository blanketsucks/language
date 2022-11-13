#ifndef _MANGLER_H
#define _MANGLER_H

#include "llvm.h"
#include "objects.h"

class Mangler {
public:
    static const char PREFIX = 'P';
    static const char SEPARATOR = '3';
    static const char ARGS_SEPARATOR = '.';
    static const char VARIADIC = 'e';
    static const char END = 'E';

    // replaces all of the . with SEPARATOR
    static std::string replace(std::string str);

    static std::string mangle(llvm::Type* type);
    static std::string mangle(
        std::string name, 
        std::vector<llvm::Type*> args, 
        bool is_variadic = false,
        llvm::Type* ret = nullptr,
        utils::Shared<Namespace> ns = nullptr, 
        utils::Shared<Struct> structure = nullptr, 
        utils::Shared<Module> module = nullptr
    );

    static std::string demangle_type(std::string name);
    static std::string demangle(std::string name);

};

#endif