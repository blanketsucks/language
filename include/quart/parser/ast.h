#ifndef _AST_H
#define _AST_H

#include <quart/lexer/tokens.h>
#include <quart/utils/pointer.h>
#include <quart/parser/attrs.h>

#include <iostream>
#include <memory>
#include <vector>
#include <cstdarg>
#include <deque>

class Visitor;
class Value;

namespace ast {

class Expr;

enum class ExternLinkageSpecifier {
    None,
    Unspecified,
    C,
};

enum class ExprKind {
    Block,
    Integer,
    Char,
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
    Import,
    Ternary,
    Foreach,
    Type,
    ArrayFill,
    TypeAlias,
    StaticAssert,
    Maybe,
    Module,
    Impl
};

enum class TypeKind {
    Builtin,
    Integer,
    Named,
    Tuple,
    Array,
    Pointer,
    Function,
    Reference
};

enum class BuiltinType {
    Bool,
    i8,
    i16,
    i32,
    i64,
    i128,
    f32,
    f64,
    Void
};

struct Attributes {
    std::map<Attribute::Type, Attribute> values;

    void add(const Attribute& attr) {
        values[attr.type] = attr;
    }

    bool has(Attribute::Type type) const {
        return this->values.find(type) != values.end();
    }

    Attribute& get(Attribute::Type type) {
        return this->values[type];
    }

    void update(const Attributes& other) {
        this->values.insert(other.values.begin(), other.values.end());
    }
};

class TypeExpr;

class Expr {
public:
    Span span;
    Attributes attributes;

    Expr(Span span, ExprKind kind) : span(span), _kind(kind) {
        this->attributes = Attributes();
    }

    ExprKind kind() const { return this->_kind; }

    template<typename T> T* as() {
        assert(T::classof(this) && "Invalid cast.");
        return static_cast<T*>(this);
    }

    virtual ~Expr() = default;
    virtual Value accept(Visitor& visitor) = 0;

private:
    ExprKind _kind;
};

struct Ident {
    std::string value;
    bool is_immutable;

    Span span;
};

template<class T> using ExprList = std::vector<utils::Scope<T>>;

// Designed after wabt's expression style
// Source: https://github.com/WebAssembly/wabt/blob/main/include/wabt/ir.h
template<ExprKind Kind> class ExprMixin : public Expr {
public:
    static bool classof(const Expr* expr) { return expr->kind() == Kind; }
    ExprMixin(Span span) : Expr(span, Kind) {}
};

struct Argument {
    std::string name;

    utils::Scope<TypeExpr> type;
    utils::Scope<Expr> default_value;

    bool is_self;
    bool is_kwarg;
    bool is_immutable;
    bool is_variadic;

    Span span;
};

class BlockExpr : public ExprMixin<ExprKind::Block> {
public:
    std::vector<utils::Scope<Expr>> block;

    BlockExpr(Span span, std::vector<utils::Scope<Expr>> block);
    Value accept(Visitor& visitor) override;
};

class IntegerExpr : public ExprMixin<ExprKind::Integer> {
public:
    std::string value;
    int bits;
    bool is_float;

    IntegerExpr(Span span, std::string value, int bits = 32, bool is_float = false);
    Value accept(Visitor& visitor) override;
};

class CharExpr : public ExprMixin<ExprKind::Char> {
public:
    char value;

    CharExpr(Span span, char value);
    Value accept(Visitor& visitor) override;
};

class FloatExpr : public ExprMixin<ExprKind::Float> {
public:
    double value;
    bool is_double;

    FloatExpr(Span span, double value, bool is_double);
    Value accept(Visitor& visitor) override;
};

class StringExpr : public ExprMixin<ExprKind::String> {
public:
    std::string value;

    StringExpr(Span span, std::string value);
    Value accept(Visitor& visitor) override;
};

class VariableExpr : public ExprMixin<ExprKind::Variable> {
public:
    std::string name;

    VariableExpr(Span span, std::string name);
    Value accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public ExprMixin<ExprKind::VariableAssignment> {
public:
    std::vector<Ident> names;
    utils::Scope<TypeExpr> type;
    utils::Scope<Expr> value;
    
    bool external;
    bool is_multiple_variables;

    std::string consume_rest;

    VariableAssignmentExpr(
        Span span,
        std::vector<Ident> names, 
        utils::Scope<TypeExpr> type, 
        utils::Scope<Expr> value, 
        std::string cnosume_rest,
        bool external = false,
        bool is_multiple_variables = false
    );
    
    Value accept(Visitor& visitor) override;
};

class ConstExpr : public ExprMixin<ExprKind::Const> {
public:
    std::string name;
    utils::Scope<TypeExpr> type;
    utils::Scope<Expr> value;

    ConstExpr(Span span, std::string name, utils::Scope<TypeExpr> type, utils::Scope<Expr> value);
    Value accept(Visitor& visitor) override;
};

class ArrayExpr : public ExprMixin<ExprKind::Array> {
public:
    std::vector<utils::Scope<Expr>> elements;

    ArrayExpr(Span span, std::vector<utils::Scope<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

class UnaryOpExpr : public ExprMixin<ExprKind::UnaryOp> {
public:
    utils::Scope<Expr> value;
    TokenKind op;

    UnaryOpExpr(Span span, TokenKind op, utils::Scope<Expr> value);
    Value accept(Visitor& visitor) override;
};

class BinaryOpExpr : public ExprMixin<ExprKind::BinaryOp> {
public:
    utils::Scope<Expr> left, right;
    TokenKind op;

    BinaryOpExpr(Span span, TokenKind op, utils::Scope<Expr> left, utils::Scope<Expr> right);
    Value accept(Visitor& visitor) override;
};

class InplaceBinaryOpExpr : public ExprMixin<ExprKind::InplaceBinaryOp> {
public:
    utils::Scope<Expr> left, right;
    TokenKind op;

    InplaceBinaryOpExpr(Span span, TokenKind op, utils::Scope<Expr> left, utils::Scope<Expr> right);
    Value accept(Visitor& visitor) override;
};

class CallExpr : public ExprMixin<ExprKind::Call> {
public:
    utils::Scope<ast::Expr> callee;

    std::vector<utils::Scope<Expr>> args;
    std::map<std::string, utils::Scope<Expr>> kwargs;

    CallExpr(
        Span span, 
        utils::Scope<ast::Expr> callee, 
        std::vector<utils::Scope<Expr>> args,
        std::map<std::string, utils::Scope<Expr>> kwargs
    );

    Value accept(Visitor& visitor) override;

    utils::Scope<Expr>& get(uint32_t index);
    utils::Scope<Expr>& get(const std::string& name);
};

class ReturnExpr : public ExprMixin<ExprKind::Return> {
public:
    utils::Scope<Expr> value;

    ReturnExpr(Span span, utils::Scope<Expr> value);
    Value accept(Visitor& visitor) override;
};

class PrototypeExpr : public ExprMixin<ExprKind::Prototype> {
public:
    std::string name;
    std::vector<Argument> args;
    utils::Scope<TypeExpr> return_type;

    bool is_c_variadic;
    bool is_operator;

    ExternLinkageSpecifier linkage;

    PrototypeExpr(
        Span span, 
        std::string name, 
        std::vector<Argument> args, 
        utils::Scope<TypeExpr> return_type, 
        bool is_c_variadic,
        bool is_operator,
        ExternLinkageSpecifier linkage
    );

    Value accept(Visitor& visitor) override;
};

class FunctionExpr : public ExprMixin<ExprKind::Function> {
public:
    utils::Scope<PrototypeExpr> prototype;
    std::vector<utils::Scope<Expr>> body;
    
    FunctionExpr(Span span, utils::Scope<PrototypeExpr> prototype, std::vector<utils::Scope<Expr>> body);
    Value accept(Visitor& visitor) override;
};

class DeferExpr : public ExprMixin<ExprKind::Defer> {
public:
    utils::Scope<Expr> expr;

    DeferExpr(Span span, utils::Scope<Expr> expr);
    Value accept(Visitor& visitor) override;
};

class IfExpr : public ExprMixin<ExprKind::If> {
public:
    utils::Scope<Expr> condition;
    utils::Scope<Expr> body;
    utils::Scope<Expr> ebody;

    IfExpr(Span span, utils::Scope<Expr> condition, utils::Scope<Expr> body, utils::Scope<Expr> ebody);
    Value accept(Visitor& visitor) override;
};

class WhileExpr : public ExprMixin<ExprKind::While> {
public:
    utils::Scope<Expr> condition;
    utils::Scope<BlockExpr> body;

    WhileExpr(Span span, utils::Scope<Expr> condition, utils::Scope<BlockExpr> body);
    Value accept(Visitor& visitor) override;
};

class ForExpr : public ExprMixin<ExprKind::For> {
public:
    utils::Scope<Expr> start;
    utils::Scope<Expr> end;
    utils::Scope<Expr> step;
    utils::Scope<Expr> body;

    ForExpr(
        Span span, 
        utils::Scope<Expr> start_,
        utils::Scope<Expr> end_,
        utils::Scope<Expr> step, 
        utils::Scope<Expr> body
    );
    
    Value accept(Visitor& visitor) override;  
};

class BreakExpr : public ExprMixin<ExprKind::Break> {
public:
    BreakExpr(Span span);
    Value accept(Visitor& visitor) override;  
};

class ContinueExpr : public ExprMixin<ExprKind::Continue> {
public:
    ContinueExpr(Span span);
    Value accept(Visitor& visitor) override;
};

struct StructField {
    std::string name;
    utils::Scope<TypeExpr> type;

    uint32_t index;

    bool is_private;
    bool is_readonly;
};

class StructExpr : public ExprMixin<ExprKind::Struct> {
public:
    std::string name;
    bool opaque;
    std::vector<utils::Scope<Expr>> parents;
    std::vector<StructField> fields;
    std::vector<utils::Scope<Expr>> methods;

    StructExpr(
        Span span, 
        std::string name, 
        bool opaque, 
        std::vector<utils::Scope<Expr>> parents = {}, 
        std::vector<StructField> fields = {}, 
        std::vector<utils::Scope<Expr>> methods = {}
    );

    Value accept(Visitor& visitor) override;
};

struct ConstructorField {
    std::string name;
    utils::Scope<Expr> value;
};

class ConstructorExpr : public ExprMixin<ExprKind::Constructor> {
public:
    utils::Scope<Expr> parent;
    std::vector<ConstructorField> fields;

    ConstructorExpr(Span span, utils::Scope<Expr> parent, std::vector<ConstructorField> fields);
    Value accept(Visitor& visitor) override;
};

class AttributeExpr : public ExprMixin<ExprKind::Attribute> {
public:
    utils::Scope<Expr> parent;
    std::string attribute;

    AttributeExpr(Span span, std::string attribute, utils::Scope<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class ElementExpr : public ExprMixin<ExprKind::Element> {
public:
    utils::Scope<Expr> value;
    utils::Scope<Expr> index;

    ElementExpr(Span span, utils::Scope<Expr> value, utils::Scope<Expr> index);
    Value accept(Visitor& visitor) override;
};

class CastExpr : public ExprMixin<ExprKind::Cast> {
public:
    utils::Scope<Expr> value;
    utils::Scope<TypeExpr> to;

    CastExpr(Span span, utils::Scope<Expr> value, utils::Scope<TypeExpr> to);
    Value accept(Visitor& visitor) override;
};

class SizeofExpr : public ExprMixin<ExprKind::Sizeof> {
public:
    utils::Scope<Expr> value;

    SizeofExpr(Span span, utils::Scope<Expr> value = nullptr);
    Value accept(Visitor& visitor) override;
};

class OffsetofExpr : public ExprMixin<ExprKind::Offsetof> {
public:
    utils::Scope<Expr> value;
    std::string field;

    OffsetofExpr(Span span, utils::Scope<Expr> value, std::string field);
    Value accept(Visitor& visitor) override;
};

class NamespaceExpr : public ExprMixin<ExprKind::Namespace> {
public:
    std::string name;
    std::deque<std::string> parents;
    std::vector<utils::Scope<Expr>> members;

    NamespaceExpr(
        Span span, std::string name, std::deque<std::string> parents, std::vector<utils::Scope<Expr>> members
    );
    Value accept(Visitor& visitor) override;
};

class NamespaceAttributeExpr : public ExprMixin<ExprKind::NamespaceAttribute> {
public:
    utils::Scope<Expr> parent;
    std::string attribute;

    NamespaceAttributeExpr(Span span, std::string attribute, utils::Scope<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class UsingExpr : public ExprMixin<ExprKind::Using> {
public:
    std::vector<std::string> members;
    utils::Scope<Expr> parent;

    UsingExpr(Span span, std::vector<std::string> members, utils::Scope<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class TupleExpr : public ExprMixin<ExprKind::Tuple> {
public:
    std::vector<utils::Scope<Expr>> elements;

    TupleExpr(Span span, std::vector<utils::Scope<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

struct EnumField {
    std::string name;
    utils::Scope<Expr> value = nullptr;
};

class EnumExpr : public ExprMixin<ExprKind::Enum> {
public:
    std::string name;
    utils::Scope<TypeExpr> type;
    std::vector<EnumField> fields;

    EnumExpr(Span span, std::string name, utils::Scope<TypeExpr> type, std::vector<EnumField> fields);
    Value accept(Visitor& visitor) override;
};

class ImportExpr : public ExprMixin<ExprKind::Import> {
public:
    std::string name;
    bool is_wildcard;
    bool is_relative;

    ImportExpr(
        Span span, std::string name, bool is_wildcard, bool is_relative
    );

    Value accept(Visitor& visitor) override;
};

class ModuleExpr : public ExprMixin<ExprKind::Module> {
public:
    std::string name;
    std::vector<utils::Scope<Expr>> body;

    ModuleExpr(Span span, std::string name, std::vector<utils::Scope<Expr>> body);
    Value accept(Visitor& visitor) override;
};

class TernaryExpr : public ExprMixin<ExprKind::Ternary> {
public:
    utils::Scope<Expr> condition;
    utils::Scope<Expr> true_expr;
    utils::Scope<Expr> false_expr;

    TernaryExpr(Span span, utils::Scope<Expr> condition, utils::Scope<Expr> true_expr, utils::Scope<Expr> false_expr);
    Value accept(Visitor& visitor) override;
};

class ForeachExpr : public ExprMixin<ExprKind::Foreach> {
public:
    Ident name;
    utils::Scope<Expr> iterable;
    utils::Scope<Expr> body;

    ForeachExpr(Span span, Ident name, utils::Scope<Expr> iterable, utils::Scope<Expr> body);
    Value accept(Visitor& visitor) override;
};

class TypeExpr : public ExprMixin<ExprKind::Type> {
public:
    TypeKind type;
    TypeExpr(Span span, TypeKind type) : ExprMixin(span), type(type) {}
};

class BuiltinTypeExpr : public TypeExpr {
public:
    BuiltinType value;

    BuiltinTypeExpr(Span span, BuiltinType value);
    Value accept(Visitor& visitor) override;
};

class IntegerTypeExpr : public TypeExpr {
public:
    utils::Scope<Expr> size;

    IntegerTypeExpr(Span span, utils::Scope<Expr> size);
    Value accept(Visitor& visitor) override;
};

class NamedTypeExpr : public TypeExpr {
public:
    std::string name;
    std::deque<std::string> parents;

    NamedTypeExpr(Span span, std::string name, std::deque<std::string> parents);
    Value accept(Visitor& visitor) override;
};

class TupleTypeExpr : public TypeExpr {
public:
    std::vector<utils::Scope<TypeExpr>> elements;

    TupleTypeExpr(Span span, std::vector<utils::Scope<TypeExpr>> elements);
    Value accept(Visitor& visitor) override;
};

class ArrayTypeExpr : public TypeExpr {
public:
    utils::Scope<TypeExpr> element;
    utils::Scope<Expr> size;

    ArrayTypeExpr(Span span, utils::Scope<TypeExpr> element, utils::Scope<Expr> size);
    Value accept(Visitor& visitor) override;
};

class PointerTypeExpr : public TypeExpr {
public:
    utils::Scope<TypeExpr> element;

    PointerTypeExpr(Span span, utils::Scope<TypeExpr> element);
    Value accept(Visitor& visitor) override;
};

class FunctionTypeExpr : public TypeExpr {
public:
    std::vector<utils::Scope<TypeExpr>> args;
    utils::Scope<TypeExpr> ret;

    FunctionTypeExpr(Span span, std::vector<utils::Scope<TypeExpr>> args, utils::Scope<TypeExpr> ret);
    Value accept(Visitor& visitor) override;
};

class ReferenceTypeExpr : public TypeExpr {
public:
    utils::Scope<TypeExpr> type;
    bool is_immutable;

    ReferenceTypeExpr(Span span, utils::Scope<TypeExpr> type, bool is_immutable);
    Value accept(Visitor& visitor) override;
};

class ArrayFillExpr : public ExprMixin<ExprKind::ArrayFill> {
public:
    utils::Scope<Expr> element;
    utils::Scope<Expr> count;

    ArrayFillExpr(Span span, utils::Scope<Expr> element, utils::Scope<Expr> count);
    Value accept(Visitor& visitor) override;
};

class TypeAliasExpr : public ExprMixin<ExprKind::TypeAlias> {
public:
    std::string name;
    utils::Scope<TypeExpr> type;

    TypeAliasExpr(Span span, std::string name, utils::Scope<TypeExpr> type);
    Value accept(Visitor& visitor) override;
};

class StaticAssertExpr : public ExprMixin<ExprKind::StaticAssert> {
public:
    utils::Scope<Expr> condition;
    std::string message;

    StaticAssertExpr(Span span, utils::Scope<Expr> condition, std::string message);
    Value accept(Visitor& visitor) override;
};

class MaybeExpr : public ExprMixin<ExprKind::Maybe> {
public:
    utils::Scope<Expr> value;

    MaybeExpr(Span span, utils::Scope<Expr> value);
    Value accept(Visitor& visitor) override;
};

class ImplExpr : public ExprMixin<ExprKind::Impl> {
public:
    utils::Scope<TypeExpr> type;

    ExprList<FunctionExpr> body;

    ImplExpr(Span span, utils::Scope<TypeExpr> type, ExprList<FunctionExpr> body);
    Value accept(Visitor& visitor) override;
};

};

#endif