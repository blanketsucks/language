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
    Unspecified,
    C,
};

enum class ExprKind {
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
    Tuple,
    Enum,
    Where
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
    Attributes attributes;

    Expr(Location start, Location end, ExprKind kind) : start(start), end(end), _kind(kind) {}

    ExprKind kind() const { return this->_kind; }
    bool is_constant() { 
        return this->kind() == ExprKind::Integer || this->kind() == ExprKind::Float || this->kind() == ExprKind::String;
    }

    template<typename T> T* cast() {
        assert(T::classof(this) && "Invalid cast.");
        return static_cast<T*>(this);
    }

    virtual ~Expr() = default;
    virtual Value accept(Visitor& visitor) = 0;

private:
    ExprKind _kind;
};

// Designed after wabt's expression style
// Source: https://github.com/WebAssembly/wabt/blob/main/src/ir.h
template<ExprKind Kind> class ExprMixin : public Expr {
public:
    static bool classof(const Expr* expr) { return expr->kind() == Kind; }
    ExprMixin(Location start, Location end) : Expr(start, end, Kind) {}
};

struct Argument {
    std::string name;
    Type* type;
};

class BlockExpr : public ExprMixin<ExprKind::Block> {
public:
    std::vector<utils::Ref<Expr>> block;

    BlockExpr(Location start, Location end, std::vector<utils::Ref<Expr>> block);
    Value accept(Visitor& visitor) override;
};

class IntegerExpr : public ExprMixin<ExprKind::Integer> {
public:
    int value;
    int bits;

    IntegerExpr(Location start, Location end, int value, int bits = 32);
    Value accept(Visitor& visitor) override;
};

class FloatExpr : public ExprMixin<ExprKind::Float> {
public:
    float value;

    FloatExpr(Location start, Location end, float value);
    Value accept(Visitor& visitor) override;
};

class StringExpr : public ExprMixin<ExprKind::String> {
public:
    std::string value;

    StringExpr(Location start, Location end, std::string value);
    Value accept(Visitor& visitor) override;
};

class VariableExpr : public ExprMixin<ExprKind::Variable> {
public:
    std::string name;

    VariableExpr(Location start, Location end, std::string name);
    Value accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public ExprMixin<ExprKind::VariableAssignment> {
public:
    std::vector<std::string> names;
    Type* type;
    utils::Ref<Expr> value;
    bool external;
    bool is_multiple_variables;

    VariableAssignmentExpr(
        Location start, 
        Location end, 
        std::vector<std::string> names, 
        Type* type, 
        utils::Ref<Expr> value, 
        bool external = false,
        bool is_multiple_variables = false
    );
    
    Value accept(Visitor& visitor) override;
};

class ConstExpr : public ExprMixin<ExprKind::Const> {
public:
    std::string name;
    Type* type;
    utils::Ref<Expr> value;

    ConstExpr(Location start, Location end, std::string name, Type* type, utils::Ref<Expr> value);
    Value accept(Visitor& visitor) override;
};

class ArrayExpr : public ExprMixin<ExprKind::Array> {
public:
    std::vector<utils::Ref<Expr>> elements;

    ArrayExpr(Location start, Location end, std::vector<utils::Ref<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

class UnaryOpExpr : public ExprMixin<ExprKind::UnaryOp> {
public:
    utils::Ref<Expr> value;
    TokenKind op;

    UnaryOpExpr(Location start, Location end, TokenKind op, utils::Ref<Expr> value);
    Value accept(Visitor& visitor) override;
};

class BinaryOpExpr : public ExprMixin<ExprKind::BinaryOp> {
public:
    utils::Ref<Expr> left, right;
    TokenKind op;

    BinaryOpExpr(Location start, Location end, TokenKind op, utils::Ref<Expr> left, utils::Ref<Expr> right);
    Value accept(Visitor& visitor) override;
};

class InplaceBinaryOpExpr : public ExprMixin<ExprKind::InplaceBinaryOp> {
public:
    utils::Ref<Expr> left, right;
    TokenKind op;

    InplaceBinaryOpExpr(Location start, Location end, TokenKind op, utils::Ref<Expr> left, utils::Ref<Expr> right);
    Value accept(Visitor& visitor) override;
};

class CallExpr : public ExprMixin<ExprKind::Call> {
public:
    utils::Ref<ast::Expr> callee;
    std::vector<utils::Ref<Expr>> args;

    CallExpr(Location start, Location end, utils::Ref<ast::Expr> callee, std::vector<utils::Ref<Expr>> args);
    Value accept(Visitor& visitor) override;
};

class ReturnExpr : public ExprMixin<ExprKind::Return> {
public:
    utils::Ref<Expr> value;

    ReturnExpr(Location start, Location end, utils::Ref<Expr> value);
    Value accept(Visitor& visitor) override;
};

class PrototypeExpr : public ExprMixin<ExprKind::Prototype> {
public:
    std::string name;
    std::vector<Argument> args;
    bool is_variadic;
    Type* return_type;
    ExternLinkageSpecifier linkage;

    PrototypeExpr(
        Location start, 
        Location end, 
        std::string name, 
        std::vector<Argument> args, 
        bool is_variadic,
        Type* return_type, 
        ExternLinkageSpecifier linkage
    );

    Value accept(Visitor& visitor) override;
};

class FunctionExpr : public ExprMixin<ExprKind::Function> {
public:
    utils::Ref<PrototypeExpr> prototype;
    utils::Ref<Expr> body;
    
    FunctionExpr(Location start, Location end, utils::Ref<PrototypeExpr> prototype, utils::Ref<Expr> body);
    Value accept(Visitor& visitor) override;
};

class DeferExpr : public ExprMixin<ExprKind::Defer> {
public:
    utils::Ref<Expr> expr;

    DeferExpr(Location start, Location end, utils::Ref<Expr> expr);
    Value accept(Visitor& visitor) override;
};

class IfExpr : public ExprMixin<ExprKind::If> {
public:
    utils::Ref<Expr> condition;
    utils::Ref<Expr> body;
    utils::Ref<Expr> ebody;

    IfExpr(Location start, Location end, utils::Ref<Expr> condition, utils::Ref<Expr> body, utils::Ref<Expr> ebody);
    Value accept(Visitor& visitor) override;
};

class WhileExpr : public ExprMixin<ExprKind::While> {
public:
    utils::Ref<Expr> condition;
    utils::Ref<BlockExpr> body;

    WhileExpr(Location start, Location end, utils::Ref<Expr> condition, utils::Ref<BlockExpr> body);
    Value accept(Visitor& visitor) override;
};

class ForExpr : public ExprMixin<ExprKind::For> {
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

class BreakExpr : public ExprMixin<ExprKind::Break> {
public:
    BreakExpr(Location start, Location end);
    Value accept(Visitor& visitor) override;  
};

class ContinueExpr : public ExprMixin<ExprKind::Continue> {
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

class StructExpr : public ExprMixin<ExprKind::Struct> {
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

class ConstructorExpr : public ExprMixin<ExprKind::Constructor> {
public:
    utils::Ref<Expr> parent;
    std::map<std::string, utils::Ref<Expr>> fields;

    ConstructorExpr(Location start, Location end, utils::Ref<Expr> parent, std::map<std::string, utils::Ref<Expr>> fields);
    Value accept(Visitor& visitor) override;
};

class AttributeExpr : public ExprMixin<ExprKind::Attribute> {
public:
    utils::Ref<Expr> parent;
    std::string attribute;

    AttributeExpr(Location start, Location end, std::string attribute, utils::Ref<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class ElementExpr : public ExprMixin<ExprKind::Element> {
public:
    utils::Ref<Expr> value;
    utils::Ref<Expr> index;

    ElementExpr(Location start, Location end, utils::Ref<Expr> value, utils::Ref<Expr> index);
    Value accept(Visitor& visitor) override;
};

class CastExpr : public ExprMixin<ExprKind::Cast> {
public:
    utils::Ref<Expr> value;
    Type* to;

    CastExpr(Location start, Location end, utils::Ref<Expr> value, Type* to);
    Value accept(Visitor& visitor) override;
};

class SizeofExpr : public ExprMixin<ExprKind::Sizeof> {
public:
    Type* type;
    utils::Ref<Expr> value;

    SizeofExpr(Location start, Location end, Type* type, utils::Ref<Expr> value = nullptr);
    Value accept(Visitor& visitor) override;
};

class OffsetofExpr : public ExprMixin<ExprKind::Offsetof> {
public:
    utils::Ref<Expr> value;
    std::string field;

    OffsetofExpr(Location start, Location end, utils::Ref<Expr> value, std::string field);
    Value accept(Visitor& visitor) override;
};

class NamespaceExpr : public ExprMixin<ExprKind::Namespace> {
public:
    std::string name;
    std::deque<std::string> parents;
    std::vector<utils::Ref<Expr>> members;

    NamespaceExpr(
        Location start, Location end, std::string name, std::deque<std::string> parents, std::vector<utils::Ref<Expr>> members
    );
    Value accept(Visitor& visitor) override;
};

class NamespaceAttributeExpr : public ExprMixin<ExprKind::NamespaceAttribute> {
public:
    utils::Ref<Expr> parent;
    std::string attribute;

    NamespaceAttributeExpr(Location start, Location end, std::string attribute, utils::Ref<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class UsingExpr : public ExprMixin<ExprKind::Using> {
public:
    std::vector<std::string> members;
    utils::Ref<Expr> parent;

    UsingExpr(Location start, Location end, std::vector<std::string> members, utils::Ref<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class TupleExpr : public ExprMixin<ExprKind::Tuple> {
public:
    std::vector<utils::Ref<Expr>> elements;

    TupleExpr(Location start, Location end, std::vector<utils::Ref<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

struct EnumField {
    std::string name;
    utils::Ref<Expr> value = nullptr;
};

class EnumExpr : public ExprMixin<ExprKind::Enum> {
public:
    std::string name;
    Type* type;
    std::vector<EnumField> fields;

    EnumExpr(Location start, Location end, std::string name, Type* type, std::vector<EnumField> fields);
    Value accept(Visitor& visitor) override;
};

class WhereExpr : public ExprMixin<ExprKind::Where> {
public:
    utils::Ref<Expr> expr;

    WhereExpr(Location start, Location end, utils::Ref<Expr> expr);
    Value accept(Visitor& visitor) override;
};

}

#endif