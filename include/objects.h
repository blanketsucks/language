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

struct FunctionArgument {
    std::string name;
    llvm::Type* type;
    bool is_reference;
    bool is_kwarg;

};

struct Function {
    std::string name;
    llvm::Type* ret;

    llvm::Function* value;

    std::vector<FunctionArgument> args;
    std::map<std::string, FunctionArgument> kwargs;
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
        std::vector<FunctionArgument> args,
        std::map<std::string, FunctionArgument> kwargs,
        llvm::Type* ret,
        llvm::Function* value,
        bool is_entry,
        bool is_intrinsic,
        bool is_anonymous, 
        ast::Attributes attrs
    );

    Branch* create_branch(std::string name, llvm::BasicBlock* loop = nullptr, llvm::BasicBlock* end = nullptr);
    bool has_return();

    bool has_kwarg(std::string name);
    std::vector<FunctionArgument> get_all_args();

    void defer(Visitor& visitor);
};

struct FunctionCall {
    llvm::Function* function;
    std::vector<llvm::Value*> args;
    llvm::Value* store;

    Location start;
    Location end;
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
    std::string qualified_name;

    llvm::StructType* type;

    std::map<std::string, StructField> fields;
    Scope* scope;

    std::vector<Struct*> parents;
    std::vector<Struct*> children;

    Location start;
    Location end;

    bool opaque;

    Struct(
        std::string name,
        std::string qualified_name,
        bool opaque, 
        llvm::StructType* type, 
        std::map<std::string, StructField> fields
    );

    int get_field_index(std::string name);
    StructField get_field_at(uint32_t index);
    std::vector<StructField> get_fields(bool with_private = false);

    bool has_method(std::string name);

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
    std::string qualified_name; // Used for mangling

    Scope* scope;

    Location start;
    Location end;

    Namespace(std::string name, std::string qualified_name) : name(name), qualified_name(qualified_name) {};
};

enum class ScopeType {
    Global,
    Function,
    Anonymous,
    Struct,
    Enum,
    Namespace,
    Module
};

struct Module {
    std::string name;
    std::string qualified_name;
    std::string path;

    Scope* scope;

    Module(
        std::string name, std::string qualified_name, std::string path
    ) : name(name), qualified_name(qualified_name), path(path) {};
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
    std::map<std::string, Module*> modules;

    Scope(std::string name, ScopeType type, Scope* parent = nullptr);

    Local get_local(std::string name);

    bool has_variable(std::string name);
    bool has_constant(std::string name);
    bool has_function(std::string name);
    bool has_struct(std::string name);
    bool has_enum(std::string name);
    bool has_namespace(std::string name);
    bool has_module(std::string name);

    llvm::Value* get_variable(std::string name);
    llvm::Value* get_constant(std::string name);
    Function* get_function(std::string name);
    Struct* get_struct(std::string name);
    Enum* get_enum(std::string name);
    Namespace* get_namespace(std::string name);
    Module* get_module(std::string name);

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
    Module* module;

    FunctionCall* call;

    Value(
        llvm::Value* value,
        bool is_constant = false,
        llvm::Value* parent = nullptr,
        Function* function = nullptr,
        Struct* structure = nullptr, 
        Namespace* ns = nullptr,
        Enum* enumeration = nullptr,
        Module* module = nullptr,
        FunctionCall* call = nullptr
    );

    llvm::Value* unwrap(Location location);

    llvm::Type* type();
    std::string name();

    static Value with_function(Function* function);
    static Value with_struct(Struct* structure);
    static Value with_namespace(Namespace* ns);
    static Value with_enum(Enum* enumeration);
    static Value with_module(Module* module);

    static Value as_call(FunctionCall* call);
};

#endif