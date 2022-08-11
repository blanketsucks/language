#ifndef _VALUES_H
#define _VALUES_H

#include <string>
#include <vector>
#include <map>

#include "ast.h"
#include "llvm.h"
#include "tokens.h"

class Visitor;
struct Struct;

struct Branch {
    std::string name;
    bool has_return;

    Branch(std::string name, bool has_return) : name(name), has_return(has_return) {}
};

struct Function {
    std::string name;
    llvm::Type* ret;

    llvm::Function* value;
    std::vector<llvm::Type*> args;
    std::map<int, llvm::Value*> defaults;
    std::map<std::string, llvm::AllocaInst*> locals;
    std::map<std::string, llvm::GlobalVariable*> constants;

    std::vector<Branch*> branches;
    Branch* branch;

    llvm::AllocaInst* return_value;
    llvm::BasicBlock* return_block;

    std::vector<Function*> calls;
    std::vector<ast::Expr*> defers;

    // Structure related attributes
    Struct* parent;
    bool is_private;

    ast::Attributes attrs;

    bool is_intrinsic;
    bool used;

    Function(std::string name, std::vector<llvm::Type*> args, llvm::Type* ret, bool is_intrinsic, ast::Attributes attrs);

    Branch* create_branch(std::string name);
    bool has_return();

    void defer(Visitor& visitor);
};

struct StructField {
    std::string name;
    llvm::Type* type;
    bool is_private;
};

struct Struct {
    std::string name;
    llvm::StructType* type;

    std::map<std::string, StructField> fields;
    std::map<std::string, Function*> methods;
    std::map<std::string, llvm::Value*> locals;

    std::vector<Struct*> parents;
    std::vector<Struct*> children;

    bool opaque;

    Struct(std::string name, bool opaque, llvm::StructType* type, std::map<std::string, StructField> fields);

    int get_field_index(std::string name);
    std::vector<StructField> get_fields(bool with_private = false);

    bool has_method(const std::string& name);
    llvm::Value* get_variable(std::string name);
};

struct Namespace {
    std::string name;

    std::map<std::string, Struct*> structs;
    std::map<std::string, Function*> functions;
    std::map<std::string, Namespace*> namespaces;
    std::map<std::string, llvm::Value*> locals;

    Namespace(std::string name);
};

struct Value {
    llvm::Value* value;
    llvm::Value* parent;

    Function* function;
    Struct* structure;
    Namespace* ns;

    Value(
        llvm::Value* value, 
        llvm::Value* parent = nullptr,
        Function* function = nullptr,
        Struct* structure = nullptr, 
        Namespace* ns = nullptr
    );

    llvm::Value* unwrap(Visitor* visitor, Location location);

    llvm::Type* type();
    std::string name();

    static Value with_function(llvm::Value* value, Function* function);
    static Value with_struct(Struct* structure);
    static Value with_namespace(Namespace* ns);
};

#endif