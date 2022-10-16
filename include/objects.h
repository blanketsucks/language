#ifndef _VALUES_H
#define _VALUES_H

#include "parser/ast.h"
#include "llvm.h"

#include <string>
#include <vector>
#include <map>

class Visitor;
struct Struct;
struct Scope;

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

struct FunctionReturn {
    llvm::Type* type;
    llvm::AllocaInst* value;
    llvm::BasicBlock* block;
};

struct Function {
    std::string name;
    llvm::Type* ret;

    llvm::Function* value;
    std::vector<llvm::Type*> args;
    std::map<int, llvm::Value*> defaults;

    Scope* scope;

    std::vector<Branch*> branches;
    Branch* branch;

    llvm::AllocaInst* return_value;
    llvm::BasicBlock* return_block;
    llvm::BasicBlock* current_block;

    std::vector<Function*> calls;
    std::vector<ast::Expr*> defers;

    Location start;
    Location end;

    // Structure related attributes
    Struct* parent;
    bool is_private;

    ast::Attributes attrs;

    bool is_entry;
    bool is_intrinsic;
    bool is_anonymous;
    bool used;

    Function(
        std::string name,
        std::vector<llvm::Type*> args, 
        llvm::Type* ret,
        llvm::Function* value,
        bool is_entry,
        bool is_intrinsic,
        bool is_anonymous, 
        ast::Attributes attrs
    );

    Branch* create_branch(std::string name, llvm::BasicBlock* loop = nullptr, llvm::BasicBlock* end = nullptr);
    bool has_return();

    void defer(Visitor& visitor);
};

struct FunctionCall {
    llvm::Function* function;
    std::vector<llvm::Value*> args;
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

    Location start;
    Location end;

    bool opaque;

    Struct(std::string name, bool opaque, llvm::StructType* type, std::map<std::string, StructField> fields);

    int get_field_index(std::string name);
    StructField get_field_at(uint32_t index);

    std::vector<StructField> get_fields(bool with_private = false);

    bool has_method(std::string name);
    llvm::Value* get_variable(std::string name);

    std::vector<Struct*> expand();
};

struct Enum {
    std::string name;
    llvm::Type* type;

    Location start;
    Location end;

    Scope* scope;

    Enum(std::string name, llvm::Type* type);

    void add_field(std::string name, llvm::Constant* value);
    bool has_field(std::string name);
    llvm::Value* get_field(std::string name);
};

struct Namespace {
    std::string name;

    Scope* scope;

    Location start;
    Location end;

    Namespace(std::string name) : name(name) {};
};

enum class ScopeType {
    Global,
    Function,
    Anonymous,
    Struct,
    Enum,
    Namespace
};

struct Scope {
    using Local = std::pair<llvm::Value*, bool>;

    std::string name;
    ScopeType type;

    Scope* parent;
    std::vector<Scope*> children;

    std::map<std::string, llvm::Value*> variables;
    std::map<std::string, llvm::Value*> constants;
    std::map<std::string, Function*> functions;
    std::map<std::string, Struct*> structs;
    std::map<std::string, Enum*> enums;
    std::map<std::string, Namespace*> namespaces;

    Scope(std::string name, ScopeType type, Scope* parent = nullptr);

    Local get_local(std::string name);

    bool has_variable(std::string name);
    bool has_constant(std::string name);
    bool has_function(std::string name);
    bool has_struct(std::string name);
    bool has_enum(std::string name);
    bool has_namespace(std::string name);

    llvm::Value* get_variable(std::string name);
    llvm::Value* get_constant(std::string name);
    Function* get_function(std::string name);
    Struct* get_struct(std::string name);
    Enum* get_enum(std::string name);
    Namespace* get_namespace(std::string name);

    std::vector<Function*> get_functions();
    std::vector<Struct*> get_structs();
    std::vector<Enum*> get_enums();
    std::vector<Namespace*> get_namespaces();

    void exit(Visitor* visitor);
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