#ifndef _AST_H
#define _AST_H

#include "types/include.h"
#include "lexer/tokens.h"
#include "llvm.h"
#include "utils.h"

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
        Break,
        Continue,
        Struct,
        Constructor,
        Attribute,
        Element,
        Cast,
        Sizeof,
        Offsetof,
        Assembly,
        Namespace,
        NamespaceAttribute,
        Using,
        Import
    };

    Value value;

    ExprKind(Value value);

    bool operator==(Value other);
    bool operator==(ExprKind other);
    bool operator!=(Value other);
    bool operator!=(ExprKind other);

    bool in(ExprKind other, ...);
    bool in(std::vector<ExprKind> others);
};

struct Attributes {
    std::vector<std::string> names;

    Attributes() { this->names = {}; }
    Attributes(std::vector<std::string> names) : names(names) {}

    void add(std::string name) { this->names.push_back(name); }
    void update(Attributes other) { this->names.insert(this->names.end(), other.names.begin(), other.names.end()); }
    bool has(std::string name) { return std::find(this->names.begin(), this->names.end(), name) != this->names.end(); }
};

class Expr {
public:
    Location start;
    Location end;
    ExprKind kind;
    Attributes attributes;

    Expr(Location start, Location end, ExprKind kind) : start(start), end(end), kind(kind) {}

    template<class T> T* cast() { return (T*)this;
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
    std::vector<utils::Ref<Expr>> block;

    BlockExpr(Location start, Location end, std::vector<utils::Ref<Expr>> block);

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
    utils::Ref<Expr> value;
    bool external;

    VariableAssignmentExpr(Location start, Location end, std::string name, Type* type, utils::Ref<Expr> value, bool external = false);
    
    Value accept(Visitor& visitor) override;
    
};

class ConstExpr : public VariableAssignmentExpr {
public:
    ConstExpr(Location start, Location end, std::string name, Type* type, utils::Ref<Expr> value);
    
    Value accept(Visitor& visitor) override;
    
};

class ArrayExpr : public Expr {
public:
    std::vector<utils::Ref<Expr>> elements;

    ArrayExpr(Location start, Location end, std::vector<utils::Ref<Expr>> elements);
    
    Value accept(Visitor& visitor) override;
    
};

class UnaryOpExpr : public Expr {
public:
    utils::Ref<Expr> value;
    TokenKind op;

    UnaryOpExpr(Location start, Location end, TokenKind op, utils::Ref<Expr> value);
    
    Value accept(Visitor& visitor) override;
    
};

class BinaryOpExpr : public Expr {
public:
    utils::Ref<Expr> left, right;
    TokenKind op;

    BinaryOpExpr(Location start, Location end, TokenKind op, utils::Ref<Expr> left, utils::Ref<Expr> right);

    Value accept(Visitor& visitor) override;
    
};

class InplaceBinaryOpExpr : public Expr {
public:
    utils::Ref<Expr> left, right;
    TokenKind op;

    InplaceBinaryOpExpr(Location start, Location end, TokenKind op, utils::Ref<Expr> left, utils::Ref<Expr> right);
    
    Value accept(Visitor& visitor) override;
    
};

class CallExpr : public Expr {
public:
    utils::Ref<ast::Expr> callee;
    std::vector<utils::Ref<Expr>> args;

    CallExpr(Location start, Location end, utils::Ref<ast::Expr> callee, std::vector<utils::Ref<Expr>> args);

    Value accept(Visitor& visitor) override;
    
};

class ReturnExpr : public Expr {
public:
    utils::Ref<Expr> value;

    ReturnExpr(Location start, Location end, utils::Ref<Expr> value);

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
    utils::Ref<PrototypeExpr> prototype;
    utils::Ref<BlockExpr> body;
    
    FunctionExpr(Location start, Location end, utils::Ref<PrototypeExpr> prototype, utils::Ref<BlockExpr> body);
    
    Value accept(Visitor& visitor) override;
    
};

class DeferExpr : public Expr {
public:
    utils::Ref<Expr> expr;

    DeferExpr(Location start, Location end, utils::Ref<Expr> expr);

    Value accept(Visitor& visitor) override;
    
};

class IfExpr : public Expr {
public:
    utils::Ref<Expr> condition;
    utils::Ref<Expr> body;
    utils::Ref<Expr> ebody;

    IfExpr(Location start, Location end, utils::Ref<Expr> condition, utils::Ref<Expr> body, utils::Ref<Expr> ebody);
    
    Value accept(Visitor& visitor) override;
    
};

class WhileExpr : public Expr {
public:
    utils::Ref<Expr> condition;
    utils::Ref<BlockExpr> body;

    WhileExpr(Location start, Location end, utils::Ref<Expr> condition, utils::Ref<BlockExpr> body);
    
    Value accept(Visitor& visitor) override;
    
};

class ForExpr : public Expr {
public:
    utils::Ref<Expr> start;
    utils::Ref<Expr> end;
    utils::Ref<Expr> step;
    utils::Ref<Expr> body;

    ForExpr(
        Location start,
        Location end, 
        utils::Ref<Expr> start_,
        utils::Ref<Expr> end_,
        utils::Ref<Expr> step, 
        utils::Ref<Expr> body
    );
    
    Value accept(Visitor& visitor) override;  
};

class BreakExpr : public Expr {
public:
    BreakExpr(Location start, Location end);
    Value accept(Visitor& visitor) override;  
};

class ContinueExpr : public Expr {
public:
    ContinueExpr(Location start, Location end);
    Value accept(Visitor& visitor) override;
};

struct StructField {
    std::string name;
    Type* type;

    uint32_t index;
    bool is_private = false;
};

class StructExpr : public Expr {
public:
    std::string name;
    bool opaque;
    std::vector<utils::Ref<Expr>> parents;
    std::map<std::string, StructField> fields;
    std::vector<utils::Ref<Expr>> methods;

    StructExpr(
        Location start, 
        Location end, 
        std::string name, 
        bool opaque, 
        std::vector<utils::Ref<Expr>> parents = {}, 
        std::map<std::string, StructField> fields = {}, 
        std::vector<utils::Ref<Expr>> methods = {}
    );

    Value accept(Visitor& visitor) override;
    
};

class ConstructorExpr : public Expr {
public:
    utils::Ref<Expr> parent;
    std::map<std::string, utils::Ref<Expr>> fields;

    ConstructorExpr(Location start, Location end, utils::Ref<Expr> parent, std::map<std::string, utils::Ref<Expr>> fields);
    
    Value accept(Visitor& visitor) override;
    
};

class AttributeExpr : public Expr {
public:
    utils::Ref<Expr> parent;
    std::string attribute;

    AttributeExpr(Location start, Location end, std::string attribute, utils::Ref<Expr> parent);
    
    Value accept(Visitor& visitor) override;
    
};

class ElementExpr : public Expr {
public:
    utils::Ref<Expr> value;
    utils::Ref<Expr> index;

    ElementExpr(Location start, Location end, utils::Ref<Expr> value, utils::Ref<Expr> index);
    
    Value accept(Visitor& visitor) override;
    
};

class CastExpr : public Expr {
public:
    utils::Ref<Expr> value;
    Type* to;

    CastExpr(Location start, Location end, utils::Ref<Expr> value, Type* to);
    
    Value accept(Visitor& visitor) override;
    
};

class SizeofExpr : public Expr {
public:
    Type* type;
    utils::Ref<Expr> value;

    SizeofExpr(Location start, Location end, Type* type, utils::Ref<Expr> value = nullptr);
    
    Value accept(Visitor& visitor) override;
};

class OffsetofExpr : public Expr {
public:
    utils::Ref<Expr> value;
    std::string field;

    OffsetofExpr(Location start, Location end, utils::Ref<Expr> value, std::string field);
    Value accept(Visitor& visitor) override;
};

class NamespaceExpr : public Expr {
public:
    std::string name;
    
    std::vector<utils::Ref<Expr>> members;

    NamespaceExpr(Location start, Location end, std::string name, std::vector<utils::Ref<Expr>> members);
    
    Value accept(Visitor& visitor) override;
    
};

class NamespaceAttributeExpr : public Expr {
public:
    utils::Ref<Expr> parent;
    std::string attribute;

    NamespaceAttributeExpr(Location start, Location end, std::string attribute, utils::Ref<Expr> parent);
    
    Value accept(Visitor& visitor) override;
    
};

class UsingExpr : public Expr {
public:
    std::vector<std::string> members;
    utils::Ref<Expr> parent;

    UsingExpr(Location start, Location end, std::vector<std::string> members, utils::Ref<Expr> parent);
    
    Value accept(Visitor& visitor) override;
    
};

}

#endif