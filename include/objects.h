#ifndef _VALUES_H
#define _VALUES_H

#include "parser/ast.h"
#include "llvm.h"

#include <string>
#include <vector>
#include <map>

class Visitor;
struct Struct;

struct Branch {
    std::string name;

    bool has_return;
    bool has_break;
    bool has_continue;

    llvm::BasicBlock* loop;
    llvm::BasicBlock* end;

    Branch(std::string name) : name(name), has_return(false), has_break(false), has_continue(false) {}

    bool has_jump() { return this->has_return || this->has_break || this->has_continue; }
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

    Branch* create_branch(std::string name, llvm::BasicBlock* loop = nullptr, llvm::BasicBlock* end = nullptr);
    bool has_return();

    void defer(Visitor& visitor);
};

struct StructField {
    std::string name;
    llvm::Type* type;
    
    bool is_private;

    uint32_t index;
    uint32_t offset;
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
    StructField get_field_at(uint32_t index);

    std::vector<StructField> get_fields(bool with_private = false);

    bool has_method(const std::string& name);
    llvm::Value* get_variable(std::string name);

    std::vector<Struct*> expand();
};

struct Enum {
    std::string name;
    llvm::Type* type;

    std::map<std::string, llvm::Constant*> fields;

    Enum(std::string name, llvm::Type* type);

    void add_field(std::string name, llvm::Constant* value);
    bool has_field(std::string name);
    llvm::Constant* get_field(std::string name);
};

struct Namespace {
    std::string name;

    std::map<std::string, Struct*> structs;
    std::map<std::string, Function*> functions;
    std::map<std::string, Namespace*> namespaces;
    std::map<std::string, llvm::Constant*> constants;

    Namespace(std::string name);
};

struct Value {
    llvm::Value* value;
    llvm::Value* parent;

    bool is_constant;

    Function* function;
    Struct* structure;
    Namespace* ns;
    Enum* enumeration;

    Value(
        llvm::Value* value,
        bool is_constant = false,
        llvm::Value* parent = nullptr,
        Function* function = nullptr,
        Struct* structure = nullptr, 
        Namespace* ns = nullptr,
        Enum* enumeration = nullptr
    );

    llvm::Value* unwrap(Location location);

    llvm::Type* type();
    std::string name();

    static Value with_function(Function* function);
    static Value with_struct(Struct* structure);
    static Value with_namespace(Namespace* ns);
    static Value with_enum(Enum* enumeration);
};

#endif