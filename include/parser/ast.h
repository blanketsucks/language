#ifndef _AST_H
#define _AST_H

#include "types/include.h"
#include "lexer/tokens.h"
#include "llvm.h"

#include <iostream>
#include <memory>
#include <vector>
#include <cstdarg>

class Visitor;
class Value;

namespace ast {

enum class ExternLinkageSpecifier {
    None,
    C,
};

class ExprKind {
public:
    enum Value {
        Unknown = -1,

        Block,
        Integer,
        Float,
        String,
        Variable,
        VariableAssignment,
        Const,
        Array,
        UnaryOp,
        BinaryOp,
        InplaceBinaryOp,
        Call,
        Return,
        Prototype,
        Function,
        Defer,
        If,
        While,
        For,
        Struct,
        Constructor,
        Attribute,
        Element,
        Cast,
        Sizeof,
        Assembly,
        Namespace,
        NamespaceAttribute,
        Using
    };

    Value value;

    ExprKind(Value value);

    bool operator==(Value other);
    bool operator==(ExprKind other);
    bool operator!=(Value other);

    bool in(ExprKind other, ...);
    bool in(std::vector<ExprKind> others);
};

struct Attributes {
    std::vector<std::string> names;

    Attributes() { this->names = {}; }
    Attributes(std::vector<std::string> names) : names(names) {}

    void add(std::string name) { this->names.push_back(name); }
    bool has(std::string name) { return std::find(this->names.begin(), this->names.end(), name) != this->names.end(); }
};

class Expr {
public:
    Location start;
    Location end;
    ExprKind kind;
    Attributes attributes;

    Expr(Location start, Location end, ExprKind kind) : start(start), end(end), kind(kind) {}

    template<class T> T* cast() {
        T* value = dynamic_cast<T*>(this);
        assert(value != nullptr && "Invalid cast");

        return value; 
    }

    bool is_constant() { return this->kind.in({ExprKind::Integer, ExprKind::Float, ExprKind::String}); }

    virtual ~Expr() = default;
    virtual Value accept(Visitor& visitor) = 0;
};

struct Argument {
    std::string name;
    Type* type;
};

class BlockExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> block;

    BlockExpr(Location start, Location end, std::vector<std::unique_ptr<Expr>> block);
    Value accept(Visitor& visitor) override;
};

class IntegerExpr : public Expr {
public:
    int value;
    int bits;

    IntegerExpr(Location start, Location end, int value, int bits = 32);
    Value accept(Visitor& visitor) override;
};

class FloatExpr : public Expr {
public:
    float value;

    FloatExpr(Location start, Location end, float value);
    Value accept(Visitor& visitor) override;
};

class StringExpr : public Expr {
public:
    std::string value;

    StringExpr(Location start, Location end, std::string value);
    Value accept(Visitor& visitor) override;
};

class VariableExpr : public Expr {
public:
    std::string name;

    VariableExpr(Location start, Location end, std::string name);
    Value accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public Expr {
public:
    std::string name;
    Type* type;
    std::unique_ptr<Expr> value;
    bool external;

    VariableAssignmentExpr(Location start, Location end, std::string name, Type* type, std::unique_ptr<Expr> value, bool external = false);
    Value accept(Visitor& visitor) override;
};

class ConstExpr : public VariableAssignmentExpr {
public:
    ConstExpr(Location start, Location end, std::string name, Type* type, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class ArrayExpr : public Expr {
public:
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayExpr(Location start, Location end, std::vector<std::unique_ptr<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

class UnaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> value;
    TokenType op;

    UnaryOpExpr(Location start, Location end, TokenType op, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class BinaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> left, right;
    TokenType op;

    BinaryOpExpr(Location start, Location end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    Value accept(Visitor& visitor) override;
};

class InplaceBinaryOpExpr : public Expr {
public:
    std::unique_ptr<Expr> left, right;
    TokenType op;

    InplaceBinaryOpExpr(Location start, Location end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    Value accept(Visitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::unique_ptr<ast::Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;

    CallExpr(Location start, Location end, std::unique_ptr<ast::Expr> callee, std::vector<std::unique_ptr<Expr>> args);
    Value accept(Visitor& visitor) override;
};

class ReturnExpr : public Expr {
public:
    std::unique_ptr<Expr> value;

    ReturnExpr(Location start, Location end, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class PrototypeExpr : public Expr {
public:
    std::string name;
    std::vector<Argument> args;
    bool has_varargs;
    Type* return_type;
    ExternLinkageSpecifier linkage_specifier = ExternLinkageSpecifier::None;

    PrototypeExpr(Location start, Location end, std::string name, Type* return_type, std::vector<Argument> args, bool has_varargs);
    Value accept(Visitor& visitor) override;
};

class FunctionExpr : public Expr {
public:
    std::unique_ptr<PrototypeExpr> prototype;
    std::unique_ptr<BlockExpr> body;
    
    FunctionExpr(Location start, Location end, std::unique_ptr<PrototypeExpr> prototype, std::unique_ptr<BlockExpr> body);
    Value accept(Visitor& visitor) override;
};

class DeferExpr : public Expr {
public:
    std::unique_ptr<Expr> expr;

    DeferExpr(Location start, Location end, std::unique_ptr<Expr> expr);
    Value accept(Visitor& visitor) override;
};

class IfExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockExpr> body;
    std::unique_ptr<BlockExpr> ebody;

    IfExpr(Location start, Location end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body, std::unique_ptr<BlockExpr> ebody);
    Value accept(Visitor& visitor) override;
};

class WhileExpr : public Expr {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockExpr> body;

    WhileExpr(Location start, Location end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body);
    Value accept(Visitor& visitor) override;
};

class ForExpr : public Expr {
public:
    std::string name;
    std::unique_ptr<Expr> iterator;
    std::unique_ptr<BlockExpr> body;

    ForExpr(Location start, Location end, std::string name, std::unique_ptr<Expr> iterator, std::unique_ptr<BlockExpr> body);
    Value accept(Visitor& visitor) override;
};

struct StructField {
    std::string name;
    Type* type;
    bool is_private = false;
};

class StructExpr : public Expr {
public:
    std::string name;
    bool packed;
    bool opaque;
    std::vector<std::unique_ptr<Expr>> parents;
    std::map<std::string, StructField> fields;
    std::vector<std::unique_ptr<Expr>> methods;

    StructExpr(
        Location start, 
        Location end, 
        std::string name, 
        bool packed, 
        bool opaque, 
        std::vector<std::unique_ptr<Expr>> parents = {}, 
        std::map<std::string, StructField> fields = {}, 
        std::vector<std::unique_ptr<Expr>> methods = {}
    );

    Value accept(Visitor& visitor) override;
};

class ConstructorExpr : public Expr {
public:
    std::unique_ptr<Expr> parent;
    std::map<std::string, std::unique_ptr<Expr>> fields;

    ConstructorExpr(Location start, Location end, std::unique_ptr<Expr> parent, std::map<std::string, std::unique_ptr<Expr>> fields);
    Value accept(Visitor& visitor) override;
};

class AttributeExpr : public Expr {
public:
    std::unique_ptr<Expr> parent;
    std::string attribute;

    AttributeExpr(Location start, Location end, std::string attribute, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class ElementExpr : public Expr {
public:
    std::unique_ptr<Expr> value;
    std::unique_ptr<Expr> index;

    ElementExpr(Location start, Location end, std::unique_ptr<Expr> value, std::unique_ptr<Expr> index);
    Value accept(Visitor& visitor) override;
};

class CastExpr : public Expr {
public:
    std::unique_ptr<Expr> value;
    Type* to;

    CastExpr(Location start, Location end, std::unique_ptr<Expr> value, Type* to);
    Value accept(Visitor& visitor) override;
};

class SizeofExpr : public Expr {
public:
    Type* type;

    SizeofExpr(Location start, Location end, Type* type);
    Value accept(Visitor& visitor) override;
};

typedef std::vector<std::pair<std::string, Expr*>> InlineAssemblyConstraint;

class InlineAssemblyExpr : public Expr {
public:
    std::string assembly;
    InlineAssemblyConstraint inputs;
    InlineAssemblyConstraint outputs;
    std::vector<std::string> clobbers;

    InlineAssemblyExpr(
        Location start,
        Location end, 
        std::string assembly, 
        InlineAssemblyConstraint inputs, 
        InlineAssemblyConstraint outputs, 
        std::vector<std::string> clobbers
    );
    Value accept(Visitor& visitor) override;
};

class NamespaceExpr : public Expr {
public:
    std::string name;
    
    std::vector<std::unique_ptr<Expr>> members;

    NamespaceExpr(Location start, Location end, std::string name, std::vector<std::unique_ptr<Expr>> members);
    Value accept(Visitor& visitor) override;
};

class NamespaceAttributeExpr : public Expr {
public:
    std::unique_ptr<Expr> parent;
    std::string attribute;

    NamespaceAttributeExpr(Location start, Location end, std::string attribute, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class UsingExpr : public Expr {
public:
    std::vector<std::string> members;
    std::unique_ptr<Expr> parent;

    UsingExpr(Location start, Location end, std::vector<std::string> members, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

}

#endif