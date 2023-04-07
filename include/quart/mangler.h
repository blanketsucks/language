#ifndef _MANGLER_H
#define _MANGLER_H

#include <quart/llvm.h>
#include <quart/objects/modules.h>
#include <quart/objects/namespaces.h>
#include <quart/objects/structs.h>

class Mangler {
public:
    static const char* PREFIX;
    static const char* SEPARATOR;
    static const char* ARGS_SEPARATOR;
    static const char* VARIADIC;
    static const char* END;

    // replaces all of the . with SEPARATOR
    static std::string replace(std::string str);

    static std::string mangle(llvm::Type* type);
    static std::string mangle(
        std::string name, 
        std::vector<llvm::Type*> args, 
        bool is_variadic = false,
        llvm::Type* ret = nullptr,
        utils::Ref<Namespace> ns = nullptr, 
        utils::Ref<Struct> structure = nullptr, 
        utils::Ref<Module> module = nullptr
    );

    static std::string demangle_type(std::string name);
    static std::string demangle(std::string name);

};

#endif