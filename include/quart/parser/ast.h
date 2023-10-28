#pragma once

#include <quart/lexer/tokens.h>
#include <quart/parser/attrs.h>

#include <memory>
#include <vector>
#include <deque>

namespace quart {

class Visitor;
struct Value;
class Type;

namespace ast {

class Expr;

enum class ExternLinkageSpecifier {
    None,
    Unspecified,
    C,
};

enum class ExprKind {
    Block,
    ExternBlock,
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
    EmptyConstructor,
    Attribute,
    Index,
    Cast,
    Sizeof,
    Offsetof,
    Assembly,
    Path,
    Using,
    Tuple,
    Enum,
    Import,
    Ternary,
    Type,
    ArrayFill,
    TypeAlias,
    StaticAssert,
    Maybe,
    Module,
    Impl,
    Match,
    RangeFor
};

enum class TypeKind {
    Builtin,
    Integer,
    Named,
    Tuple,
    Array,
    Pointer,
    Function,
    Reference,
    Generic
};

enum class BuiltinType {
    Bool,
    i8,
    i16,
    i32,
    i64,
    i128,
    u8,
    u16,
    u32,
    u64,
    u128,
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
    bool is_mutable;

    Span span;
};

template<class T> using ExprList = std::vector<std::unique_ptr<T>>;

// Designed after wabt's expression style
// Source: https://github.com/WebAssembly/wabt/blob/main/include/wabt/ir.h
template<ExprKind Kind> class ExprMixin : public Expr {
public:
    static bool classof(const Expr* expr) { return expr->kind() == Kind; }
    ExprMixin(Span span) : Expr(span, Kind) {}
};

struct Argument {
    std::string name;

    std::unique_ptr<TypeExpr> type;
    std::unique_ptr<Expr> default_value;

    bool is_self;
    bool is_kwarg;
    bool is_mutable;
    bool is_variadic;

    Span span;
};

class BlockExpr : public ExprMixin<ExprKind::Block> {
public:
    std::vector<std::unique_ptr<Expr>> block;

    BlockExpr(Span span, std::vector<std::unique_ptr<Expr>> block);
    Value accept(Visitor& visitor) override;
};

class ExternBlockExpr : public ExprMixin<ExprKind::ExternBlock> {
public:
    std::vector<std::unique_ptr<Expr>> block;

    ExternBlockExpr(Span span, std::vector<std::unique_ptr<Expr>> block);
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
    std::unique_ptr<TypeExpr> type;
    std::unique_ptr<Expr> value;
    
    bool external;
    bool is_multiple_variables;

    std::string consume_rest;

    VariableAssignmentExpr(
        Span span,
        std::vector<Ident> names, 
        std::unique_ptr<TypeExpr> type, 
        std::unique_ptr<Expr> value, 
        std::string cnosume_rest,
        bool external = false,
        bool is_multiple_variables = false
    );
    
    Value accept(Visitor& visitor) override;
};

class ConstExpr : public ExprMixin<ExprKind::Const> {
public:
    std::string name;
    std::unique_ptr<TypeExpr> type;
    std::unique_ptr<Expr> value;

    ConstExpr(Span span, std::string name, std::unique_ptr<TypeExpr> type, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class ArrayExpr : public ExprMixin<ExprKind::Array> {
public:
    std::vector<std::unique_ptr<Expr>> elements;

    ArrayExpr(Span span, std::vector<std::unique_ptr<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

class UnaryOpExpr : public ExprMixin<ExprKind::UnaryOp> {
public:
    std::unique_ptr<Expr> value;
    UnaryOp op;

    UnaryOpExpr(Span span, UnaryOp op, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class BinaryOpExpr : public ExprMixin<ExprKind::BinaryOp> {
public:
    std::unique_ptr<Expr> left, right;
    BinaryOp op;

    BinaryOpExpr(Span span, BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    Value accept(Visitor& visitor) override;
};

class InplaceBinaryOpExpr : public ExprMixin<ExprKind::InplaceBinaryOp> {
public:
    std::unique_ptr<Expr> left, right;
    BinaryOp op;

    InplaceBinaryOpExpr(Span span, BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right);
    Value accept(Visitor& visitor) override;
};

class CallExpr : public ExprMixin<ExprKind::Call> {
public:
    std::unique_ptr<ast::Expr> callee;

    std::vector<std::unique_ptr<Expr>> args;
    std::map<std::string, std::unique_ptr<Expr>> kwargs;

    CallExpr(
        Span span, 
        std::unique_ptr<ast::Expr> callee, 
        std::vector<std::unique_ptr<Expr>> args,
        std::map<std::string, std::unique_ptr<Expr>> kwargs
    );

    Value accept(Visitor& visitor) override;

    std::unique_ptr<Expr>& get(u32 index);
    std::unique_ptr<Expr>& get(const std::string& name);
};

class ReturnExpr : public ExprMixin<ExprKind::Return> {
public:
    std::unique_ptr<Expr> value;

    ReturnExpr(Span span, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class PrototypeExpr : public ExprMixin<ExprKind::Prototype> {
public:
    std::string name;
    std::vector<Argument> args;
    std::unique_ptr<TypeExpr> return_type;

    bool is_c_variadic;
    bool is_operator;

    ExternLinkageSpecifier linkage;

    PrototypeExpr(
        Span span, 
        std::string name, 
        std::vector<Argument> args, 
        std::unique_ptr<TypeExpr> return_type, 
        bool is_c_variadic,
        bool is_operator,
        ExternLinkageSpecifier linkage
    );

    Value accept(Visitor& visitor) override;
};

class FunctionExpr : public ExprMixin<ExprKind::Function> {
public:
    std::unique_ptr<PrototypeExpr> prototype;
    std::vector<std::unique_ptr<Expr>> body;
    
    FunctionExpr(Span span, std::unique_ptr<PrototypeExpr> prototype, std::vector<std::unique_ptr<Expr>> body);
    Value accept(Visitor& visitor) override;
};

class DeferExpr : public ExprMixin<ExprKind::Defer> {
public:
    std::unique_ptr<Expr> expr;

    DeferExpr(Span span, std::unique_ptr<Expr> expr);
    Value accept(Visitor& visitor) override;
};

class IfExpr : public ExprMixin<ExprKind::If> {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> body;
    std::unique_ptr<Expr> ebody;

    IfExpr(Span span, std::unique_ptr<Expr> condition, std::unique_ptr<Expr> body, std::unique_ptr<Expr> ebody);
    Value accept(Visitor& visitor) override;
};

class WhileExpr : public ExprMixin<ExprKind::While> {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockExpr> body;

    WhileExpr(Span span, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body);
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
    std::unique_ptr<TypeExpr> type;

    u32 index;

    bool is_private;
    bool is_readonly;
};

class StructExpr : public ExprMixin<ExprKind::Struct> {
public:
    std::string name;
    bool opaque;
    std::vector<StructField> fields;
    std::vector<std::unique_ptr<Expr>> methods;

    StructExpr(
        Span span, 
        std::string name,
        bool opaque,
        std::vector<StructField> fields = {}, 
        std::vector<std::unique_ptr<Expr>> methods = {}
    );

    Value accept(Visitor& visitor) override;
};

struct ConstructorField {
    std::string name;
    std::unique_ptr<Expr> value;
};

class ConstructorExpr : public ExprMixin<ExprKind::Constructor> {
public:
    std::unique_ptr<Expr> parent;
    std::vector<ConstructorField> fields;

    ConstructorExpr(Span span, std::unique_ptr<Expr> parent, std::vector<ConstructorField> fields);
    Value accept(Visitor& visitor) override;
};

class EmptyConstructorExpr : public ExprMixin<ExprKind::EmptyConstructor> {
public:
    std::unique_ptr<Expr> parent;

    EmptyConstructorExpr(Span span, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class AttributeExpr : public ExprMixin<ExprKind::Attribute> {
public:
    std::unique_ptr<Expr> parent;
    std::string attribute;

    AttributeExpr(Span span, std::string attribute, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class IndexExpr : public ExprMixin<ExprKind::Index> {
public:
    std::unique_ptr<Expr> value;
    std::unique_ptr<Expr> index;

    IndexExpr(Span span, std::unique_ptr<Expr> value, std::unique_ptr<Expr> index);
    Value accept(Visitor& visitor) override;
};

class CastExpr : public ExprMixin<ExprKind::Cast> {
public:
    std::unique_ptr<Expr> value;
    std::unique_ptr<TypeExpr> to;

    CastExpr(Span span, std::unique_ptr<Expr> value, std::unique_ptr<TypeExpr> to);
    Value accept(Visitor& visitor) override;
};

class SizeofExpr : public ExprMixin<ExprKind::Sizeof> {
public:
    std::unique_ptr<Expr> value;

    SizeofExpr(Span span, std::unique_ptr<Expr> value = nullptr);
    Value accept(Visitor& visitor) override;
};

class OffsetofExpr : public ExprMixin<ExprKind::Offsetof> {
public:
    std::unique_ptr<Expr> value;
    std::string field;

    OffsetofExpr(Span span, std::unique_ptr<Expr> value, std::string field);
    Value accept(Visitor& visitor) override;
};

class PathExpr : public ExprMixin<ExprKind::Path> {
public:
    std::unique_ptr<Expr> parent;
    std::string name;

    PathExpr(Span span, std::string name, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class UsingExpr : public ExprMixin<ExprKind::Using> {
public:
    std::vector<std::string> members;
    std::unique_ptr<Expr> parent;

    UsingExpr(Span span, std::vector<std::string> members, std::unique_ptr<Expr> parent);
    Value accept(Visitor& visitor) override;
};

class TupleExpr : public ExprMixin<ExprKind::Tuple> {
public:
    std::vector<std::unique_ptr<Expr>> elements;

    TupleExpr(Span span, std::vector<std::unique_ptr<Expr>> elements);
    Value accept(Visitor& visitor) override;
};

struct EnumField {
    std::string name;
    std::unique_ptr<Expr> value = nullptr;
};

class EnumExpr : public ExprMixin<ExprKind::Enum> {
public:
    std::string name;
    std::unique_ptr<TypeExpr> type;
    std::vector<EnumField> fields;

    EnumExpr(Span span, std::string name, std::unique_ptr<TypeExpr> type, std::vector<EnumField> fields);
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
    std::vector<std::unique_ptr<Expr>> body;

    ModuleExpr(Span span, std::string name, std::vector<std::unique_ptr<Expr>> body);
    Value accept(Visitor& visitor) override;
};

class TernaryExpr : public ExprMixin<ExprKind::Ternary> {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> true_expr;
    std::unique_ptr<Expr> false_expr;

    TernaryExpr(Span span, std::unique_ptr<Expr> condition, std::unique_ptr<Expr> true_expr, std::unique_ptr<Expr> false_expr);
    Value accept(Visitor& visitor) override;
};

class ForExpr : public ExprMixin<ExprKind::For> {
public:
    Ident name;
    std::unique_ptr<Expr> iterable;
    std::unique_ptr<Expr> body;

    ForExpr(Span span, Ident name, std::unique_ptr<Expr> iterable, std::unique_ptr<Expr> body);
    Value accept(Visitor& visitor) override;
};

class RangeForExpr : public ExprMixin<ExprKind::RangeFor> {
public:
    Ident name;
    
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
    std::unique_ptr<Expr> body;

    RangeForExpr(Span span, Ident name, std::unique_ptr<Expr> start, std::unique_ptr<Expr> end, std::unique_ptr<Expr> body);
    Value accept(Visitor& visitor) override;
};

class TypeExpr {
public:
    Span span;
    TypeKind kind;

    TypeExpr(Span span, TypeKind kind) : span(span), kind(kind) {}

    virtual Type* accept(Visitor& visitor) = 0;
};

class BuiltinTypeExpr : public TypeExpr {
public:
    BuiltinType value;

    BuiltinTypeExpr(Span span, BuiltinType value);
    Type* accept(Visitor& visitor) override;
};

class IntegerTypeExpr : public TypeExpr {
public:
    std::unique_ptr<Expr> size;

    IntegerTypeExpr(Span span, std::unique_ptr<Expr> size);
    Type* accept(Visitor& visitor) override;
};

class NamedTypeExpr : public TypeExpr {
public:
    std::string name;
    std::deque<std::string> parents;

    NamedTypeExpr(Span span, std::string name, std::deque<std::string> parents);
    Type* accept(Visitor& visitor) override;
};

class TupleTypeExpr : public TypeExpr {
public:
    std::vector<std::unique_ptr<TypeExpr>> types;

    TupleTypeExpr(Span span, std::vector<std::unique_ptr<TypeExpr>> types);
    Type* accept(Visitor& visitor) override;
};

class ArrayTypeExpr : public TypeExpr {
public:
    std::unique_ptr<TypeExpr> type;
    std::unique_ptr<Expr> size;

    ArrayTypeExpr(Span span, std::unique_ptr<TypeExpr> type, std::unique_ptr<Expr> size);
    Type* accept(Visitor& visitor) override;
};

class PointerTypeExpr : public TypeExpr {
public:
    std::unique_ptr<TypeExpr> type;
    bool is_mutable;

    PointerTypeExpr(Span span, std::unique_ptr<TypeExpr> type, bool is_mutable);
    Type* accept(Visitor& visitor) override;
};

class FunctionTypeExpr : public TypeExpr {
public:
    std::vector<std::unique_ptr<TypeExpr>> args;
    std::unique_ptr<TypeExpr> ret;

    FunctionTypeExpr(Span span, std::vector<std::unique_ptr<TypeExpr>> args, std::unique_ptr<TypeExpr> ret);
    Type* accept(Visitor& visitor) override;
};

class ReferenceTypeExpr : public TypeExpr {
public:
    std::unique_ptr<TypeExpr> type;
    bool is_mutable;

    ReferenceTypeExpr(Span span, std::unique_ptr<TypeExpr> type, bool is_mutable);
    Type* accept(Visitor& visitor) override;
};

class GenericTypeExpr : public TypeExpr {
public:
    std::unique_ptr<NamedTypeExpr> parent;
    std::vector<std::unique_ptr<TypeExpr>> args;

    GenericTypeExpr(Span span, std::unique_ptr<NamedTypeExpr> parent, std::vector<std::unique_ptr<TypeExpr>> args);
    Type* accept(Visitor& visitor) override;
};

class ArrayFillExpr : public ExprMixin<ExprKind::ArrayFill> {
public:
    std::unique_ptr<Expr> element;
    std::unique_ptr<Expr> count;

    ArrayFillExpr(Span span, std::unique_ptr<Expr> element, std::unique_ptr<Expr> count);
    Value accept(Visitor& visitor) override;
};

struct GenericParameter {
    std::string name;

    std::vector<std::unique_ptr<TypeExpr>> bounds;
    std::unique_ptr<TypeExpr> default_type;

    Span span;
};

class TypeAliasExpr : public ExprMixin<ExprKind::TypeAlias> {
public:
    std::string name;
    std::unique_ptr<TypeExpr> type;

    std::vector<GenericParameter> parameters;

    TypeAliasExpr(Span span, std::string name, std::unique_ptr<TypeExpr> type, std::vector<GenericParameter> parameters);
    Value accept(Visitor& visitor) override;

    bool is_generic_alias() const { return !this->parameters.empty(); }
};

class StaticAssertExpr : public ExprMixin<ExprKind::StaticAssert> {
public:
    std::unique_ptr<Expr> condition;
    std::string message;

    StaticAssertExpr(Span span, std::unique_ptr<Expr> condition, std::string message);
    Value accept(Visitor& visitor) override;
};

class MaybeExpr : public ExprMixin<ExprKind::Maybe> {
public:
    std::unique_ptr<Expr> value;

    MaybeExpr(Span span, std::unique_ptr<Expr> value);
    Value accept(Visitor& visitor) override;
};

class ImplExpr : public ExprMixin<ExprKind::Impl> {
public:
    std::unique_ptr<TypeExpr> type;

    ExprList<FunctionExpr> body;

    ImplExpr(Span span, std::unique_ptr<TypeExpr> type, ExprList<FunctionExpr> body);
    Value accept(Visitor& visitor) override;
};

struct MatchPattern {
    bool is_wildcard = false;
    bool is_conditional = false;

    std::vector<std::unique_ptr<Expr>> values; // A | B | C
    Span span;
};

struct MatchArm {
    MatchPattern pattern;
    std::unique_ptr<Expr> body;

    size_t index;
    
    bool is_wildcard() { return this->pattern.is_wildcard; }
};

class MatchExpr : public ExprMixin<ExprKind::Match> {
public:
    std::unique_ptr<Expr> value;
    std::vector<MatchArm> arms;

    MatchExpr(Span span, std::unique_ptr<Expr> value, std::vector<MatchArm> arms);
    Value accept(Visitor& visitor) override;
};

using TypeExprList = std::vector<std::unique_ptr<TypeExpr>>;

};

}