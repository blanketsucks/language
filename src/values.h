#ifndef _VALUES_H
#define _VALUES_H

#include <string>
#include <vector>
#include <map>

#include "llvm.h"
#include "tokens.hpp"

class Visitor;

enum class ModuleState {
    Initialized,
    Compiled
};

struct Function {
    std::string name;
    std::vector<llvm::Type*> args;
    llvm::Type* ret;
    std::map<std::string, llvm::AllocaInst*> locals;
    std::map<std::string, llvm::Value*> constants;
    bool has_return;
    bool is_intrinsic;
    bool used;

    Function(std::string name, std::vector<llvm::Type*> args, llvm::Type* ret, bool is_intrinsic);
};

struct Struct {
    std::string name;
    llvm::StructType* type;
    std::map<std::string, llvm::Type*> fields;
    std::map<std::string, Function*> methods;
    std::map<std::string, llvm::Value*> locals;
    llvm::Function* constructor;

    Struct(std::string name, llvm::StructType* type, std::map<std::string, llvm::Type*> fields);

    int get_attribute(std::string name);
    bool has_method(const std::string& name);
};

struct Module {
    std::string path;
    ModuleState state;

    bool is_ready();
};

struct Namespace {
    std::string name;
    std::map<std::string, Struct*> structs;
    std::map<std::string, Function*> functions;
    std::map<std::string, Namespace*> namespaces;

    Namespace(std::string name);
};

struct Value {
    llvm::Value* value;
    llvm::Value* parent;
    Struct* structure;
    Namespace* ns;

    Value(llvm::Value* value, llvm::Value* parent = nullptr, Struct* structure = nullptr, Namespace* ns = nullptr);

    llvm::Value* unwrap(Visitor* visitor, Location location);

    llvm::Type* type();
    std::string name();
};

#endif