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
class TypeExpr;

template<class T> using ExprList = std::vector<OwnPtr<T>>;

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
    Reference,
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

    [[nodiscard]] bool has(Attribute::Type type) const { return this->values.find(type) != values.end(); }
    [[nodiscard]] Attribute const& get(Attribute::Type type) const { return this->values.at(type); }

    void update(const Attributes& other) {
        this->values.insert(other.values.begin(), other.values.end());
    }
};

struct Ident {
    std::string value;
    bool is_mutable;

    Span span;
};

struct Argument {
    std::string name;

    OwnPtr<TypeExpr> type;
    OwnPtr<Expr> default_value;

    bool is_self;
    bool is_kwarg;
    bool is_mutable;
    bool is_variadic;

    Span span;
};

struct StructField {
    std::string name;
    OwnPtr<TypeExpr> type;

    u32 index;

    bool is_private;
    bool is_readonly;
};

struct ConstructorField {
    std::string name;
    OwnPtr<Expr> value;
};

struct EnumField {
    std::string name;
    OwnPtr<Expr> value = nullptr;
};

struct GenericParameter {
    std::string name;

    ExprList<TypeExpr> constraints;
    OwnPtr<TypeExpr> default_type;

    Span span;
};

struct MatchPattern {
    bool is_wildcard = false;
    bool is_conditional = false;

    std::vector<OwnPtr<Expr>> values; // A | B | C
    Span span;
};

struct MatchArm {
    MatchPattern pattern;
    OwnPtr<Expr> body;

    size_t index;
    
    bool is_wildcard() { return this->pattern.is_wildcard; }
};

class Expr {
public:
    Span span;
    Attributes attributes;

    Expr(Span span, ExprKind kind) : span(span), _kind(kind) {
        this->attributes = Attributes();
    }

    [[nodiscard]] ExprKind kind() const { return this->_kind; }

    [[nodiscard]] bool is(ExprKind kind) const { return this->_kind == kind; }
    template<typename... Args> [[nodiscard]] bool is(ExprKind kind, Args... args) const {
        return this->_kind == kind || this->is(args...);
    }

    template<typename T> [[nodiscard]] T* as() {
        assert(T::classof(this) && "Invalid cast.");
        return static_cast<T*>(this);
    }

    virtual ~Expr() = default;
    virtual Value accept(Visitor&) = 0;

private:
    ExprKind _kind;
};

// Designed after wabt's expression style
// Source: https://github.com/WebAssembly/wabt/blob/main/include/wabt/ir.h
template<ExprKind Kind> class ExprMixin : public Expr {
public:
    static bool classof(const Expr* expr) { return expr->kind() == Kind; }
    ExprMixin(Span span) : Expr(span, Kind) {}
};

class BlockExpr : public ExprMixin<ExprKind::Block> {
public:
    ExprList<Expr> block;

    BlockExpr(Span span, ExprList<Expr> block) : ExprMixin(span), block(std::move(block)) {}
    Value accept(Visitor& visitor) override;
};

class ExternBlockExpr : public ExprMixin<ExprKind::ExternBlock> {
public:
    ExprList<Expr> block;

    ExternBlockExpr(Span span, ExprList<Expr> block) : ExprMixin(span), block(std::move(block)) {}
    Value accept(Visitor& visitor) override;
};

class IntegerExpr : public ExprMixin<ExprKind::Integer> {
public:
    std::string value;

    u32 bits;
    bool is_float;

    IntegerExpr(Span span, std::string value, u32 bits = 32, bool is_float = false) :
        ExprMixin(span), value(std::move(value)), bits(bits), is_float(is_float) {}

    Value accept(Visitor& visitor) override;
};

class CharExpr : public ExprMixin<ExprKind::Char> {
public:
    char value;

    CharExpr(Span span, char value) : ExprMixin(span), value(value) {}
    Value accept(Visitor& visitor) override;
};

class FloatExpr : public ExprMixin<ExprKind::Float> {
public:
    double value;
    bool is_double;

    FloatExpr(Span span, double value, bool is_double) : ExprMixin(span), value(value), is_double(is_double) {}
    Value accept(Visitor& visitor) override;
};

class StringExpr : public ExprMixin<ExprKind::String> {
public:
    std::string value;

    StringExpr(Span span, std::string value) : ExprMixin(span), value(std::move(value)) {}
    Value accept(Visitor& visitor) override;
};

class VariableExpr : public ExprMixin<ExprKind::Variable> {
public:
    std::string name;

    VariableExpr(Span span, std::string name) : ExprMixin(span), name(std::move(name)) {}
    Value accept(Visitor& visitor) override;
};

class VariableAssignmentExpr : public ExprMixin<ExprKind::VariableAssignment> {
public:
    std::vector<Ident> identifiers;
    OwnPtr<TypeExpr> type;
    OwnPtr<Expr> value;
    
    bool external;

    VariableAssignmentExpr(
        Span span,
        std::vector<Ident> identifiers, 
        OwnPtr<TypeExpr> type, 
        OwnPtr<Expr> value,
        bool external = false
    ) : ExprMixin(span), identifiers(std::move(identifiers)), type(std::move(type)), 
        value(std::move(value)), external(external) {}
    
    Value accept(Visitor& visitor) override;

    [[nodiscard]] bool has_multiple_variables() const { return this->identifiers.size() > 1; }
};

class ConstExpr : public ExprMixin<ExprKind::Const> {
public:
    std::string name;
    OwnPtr<TypeExpr> type;
    OwnPtr<Expr> value;

    ConstExpr(
        Span span, std::string name, OwnPtr<TypeExpr> type, OwnPtr<Expr> value
    ) : ExprMixin(span), name(std::move(name)), type(std::move(type)), value(std::move(value)) {}

    Value accept(Visitor& visitor) override;
};

class ArrayExpr : public ExprMixin<ExprKind::Array> {
public:
    ExprList<Expr> elements;

    ArrayExpr(Span span, ExprList<Expr> elements) : ExprMixin(span), elements(std::move(elements)) {}

    Value accept(Visitor& visitor) override;
};

class UnaryOpExpr : public ExprMixin<ExprKind::UnaryOp> {
public:
    OwnPtr<Expr> value;
    UnaryOp op;

    UnaryOpExpr(Span span, OwnPtr<Expr> value, UnaryOp op)
        : ExprMixin(span), value(std::move(value)), op(op) {}

    Value accept(Visitor& visitor) override;
};

class ReferenceExpr : public ExprMixin<ExprKind::Reference> {
public:
    OwnPtr<Expr> value;
    bool is_mutable;

    ReferenceExpr(Span span, OwnPtr<Expr> value, bool is_mutable) :
        ExprMixin(span), value(std::move(value)), is_mutable(is_mutable) {}

    Value accept(Visitor& visitor) override;
};

class BinaryOpExpr : public ExprMixin<ExprKind::BinaryOp> {
public:
    OwnPtr<Expr> left, right;
    BinaryOp op;

    BinaryOpExpr(Span span, BinaryOp op, OwnPtr<Expr> left, OwnPtr<Expr> right) :
        ExprMixin(span), left(std::move(left)), right(std::move(right)), op(op) {}

    Value accept(Visitor& visitor) override;
};

class InplaceBinaryOpExpr : public ExprMixin<ExprKind::InplaceBinaryOp> {
public:
    OwnPtr<Expr> left, right;
    BinaryOp op;

    InplaceBinaryOpExpr(Span span, BinaryOp op, OwnPtr<Expr> left, OwnPtr<Expr> right) :
        ExprMixin(span), left(std::move(left)), right(std::move(right)), op(op) {}

    Value accept(Visitor& visitor) override;
};

class CallExpr : public ExprMixin<ExprKind::Call> {
public:
    OwnPtr<ast::Expr> callee;

    ExprList<Expr> args;
    std::map<std::string, OwnPtr<Expr>> kwargs;

    CallExpr(
        Span span, 
        OwnPtr<ast::Expr> callee, 
        ExprList<Expr> args,
        std::map<std::string, OwnPtr<Expr>> kwargs
    ) : ExprMixin(span), callee(std::move(callee)), args(std::move(args)), kwargs(std::move(kwargs)) {}

    Value accept(Visitor& visitor) override;

    OwnPtr<Expr>& get(u32 index) { return this->args[index]; }
    OwnPtr<Expr>& get(const std::string& name) { return this->kwargs[name]; }
};

class ReturnExpr : public ExprMixin<ExprKind::Return> {
public:
    OwnPtr<Expr> value;

    ReturnExpr(Span span, OwnPtr<Expr> value) : ExprMixin(span), value(std::move(value)) {}
    Value accept(Visitor& visitor) override;
};

class PrototypeExpr : public ExprMixin<ExprKind::Prototype> {
public:
    std::string name;
    std::vector<Argument> args;
    OwnPtr<TypeExpr> return_type;

    bool is_c_variadic;
    bool is_operator;

    ExternLinkageSpecifier linkage;

    PrototypeExpr(
        Span span, 
        std::string name, 
        std::vector<Argument> args, 
        OwnPtr<TypeExpr> return_type, 
        bool is_c_variadic,
        bool is_operator,
        ExternLinkageSpecifier linkage
    ) : ExprMixin(span), name(std::move(name)), args(std::move(args)), return_type(std::move(return_type)), 
        is_c_variadic(is_c_variadic), is_operator(is_operator), linkage(linkage) {}

    Value accept(Visitor& visitor) override;
};

class FunctionExpr : public ExprMixin<ExprKind::Function> {
public:
    OwnPtr<PrototypeExpr> prototype;
    ExprList<Expr> body;
    
    FunctionExpr(Span span, OwnPtr<PrototypeExpr> prototype, std::vector<OwnPtr<Expr>> body) :
        ExprMixin(span), prototype(std::move(prototype)), body(std::move(body)) {}

    Value accept(Visitor& visitor) override;
};

class DeferExpr : public ExprMixin<ExprKind::Defer> {
public:
    OwnPtr<Expr> expr;

    DeferExpr(Span span, OwnPtr<Expr> expr) : ExprMixin(span), expr(std::move(expr)) {}
    Value accept(Visitor& visitor) override;
};

class IfExpr : public ExprMixin<ExprKind::If> {
public:
    OwnPtr<Expr> condition;
    OwnPtr<Expr> body;
    OwnPtr<Expr> else_body;

    IfExpr(Span span, OwnPtr<Expr> condition, OwnPtr<Expr> body, OwnPtr<Expr> else_body) :
        ExprMixin(span), condition(std::move(condition)), body(std::move(body)), else_body(std::move(else_body)) {}

    Value accept(Visitor& visitor) override;
};

class WhileExpr : public ExprMixin<ExprKind::While> {
public:
    OwnPtr<Expr> condition;
    OwnPtr<BlockExpr> body;

    WhileExpr(Span span, OwnPtr<Expr> condition, OwnPtr<BlockExpr> body) :
        ExprMixin(span), condition(std::move(condition)), body(std::move(body)) {}

    Value accept(Visitor& visitor) override;
};

class BreakExpr : public ExprMixin<ExprKind::Break> {
public:
    BreakExpr(Span span) : ExprMixin(span) {}

    Value accept(Visitor& visitor) override;  
};

class ContinueExpr : public ExprMixin<ExprKind::Continue> {
public:
    ContinueExpr(Span span) : ExprMixin(span) {}
    Value accept(Visitor& visitor) override;
};

class StructExpr : public ExprMixin<ExprKind::Struct> {
public:
    std::string name;
    bool opaque;
    std::vector<StructField> fields;
    std::vector<OwnPtr<Expr>> methods;

    StructExpr(
        Span span, 
        std::string name,
        bool opaque,
        std::vector<StructField> fields, 
        std::vector<OwnPtr<Expr>> methods
    ) : ExprMixin(span), name(std::move(name)), opaque(opaque), fields(std::move(fields)), methods(std::move(methods)) {}

    Value accept(Visitor& visitor) override;
};

class ConstructorExpr : public ExprMixin<ExprKind::Constructor> {
public:
    OwnPtr<Expr> parent;
    std::vector<ConstructorField> fields;

    ConstructorExpr(Span span, OwnPtr<Expr> parent, std::vector<ConstructorField> fields) :
        ExprMixin(span), parent(std::move(parent)), fields(std::move(fields)) {}

    Value accept(Visitor& visitor) override;
};

class EmptyConstructorExpr : public ExprMixin<ExprKind::EmptyConstructor> {
public:
    OwnPtr<Expr> parent;

    EmptyConstructorExpr(Span span, OwnPtr<Expr> parent) :
        ExprMixin(span), parent(std::move(parent)) {}

    Value accept(Visitor& visitor) override;
};

class AttributeExpr : public ExprMixin<ExprKind::Attribute> {
public:
    OwnPtr<Expr> parent;
    std::string attribute;

    AttributeExpr(Span span, OwnPtr<Expr> parent, std::string attribute) :
        ExprMixin(span), parent(std::move(parent)), attribute(std::move(attribute)) {}

    Value accept(Visitor& visitor) override;
};

class IndexExpr : public ExprMixin<ExprKind::Index> {
public:
    OwnPtr<Expr> value;
    OwnPtr<Expr> index;

    IndexExpr(Span span, OwnPtr<Expr> value, OwnPtr<Expr> index) :
        ExprMixin(span), value(std::move(value)), index(std::move(index)) {}

    Value accept(Visitor& visitor) override;
};

class CastExpr : public ExprMixin<ExprKind::Cast> {
public:
    OwnPtr<Expr> value;
    OwnPtr<TypeExpr> to;

    CastExpr(Span span, OwnPtr<Expr> value, OwnPtr<TypeExpr> to) :
        ExprMixin(span), value(std::move(value)), to(std::move(to)) {}

    Value accept(Visitor& visitor) override;
};

class SizeofExpr : public ExprMixin<ExprKind::Sizeof> {
public:
    OwnPtr<Expr> value;

    SizeofExpr(Span span, OwnPtr<Expr> value = nullptr) : ExprMixin(span), value(std::move(value)) {}
    Value accept(Visitor& visitor) override;
};

class OffsetofExpr : public ExprMixin<ExprKind::Offsetof> {
public:
    OwnPtr<Expr> value;
    std::string field;

    OffsetofExpr(Span span, OwnPtr<Expr> value, std::string field) :
        ExprMixin(span), value(std::move(value)), field(std::move(field)) {}

    Value accept(Visitor& visitor) override;
};

class PathExpr : public ExprMixin<ExprKind::Path> {
public:
    OwnPtr<Expr> parent;
    std::string name;

    PathExpr(Span span, OwnPtr<Expr> parent, std::string name) :
        ExprMixin(span), parent(std::move(parent)), name(std::move(name)) {}

    Value accept(Visitor& visitor) override;
};

class UsingExpr : public ExprMixin<ExprKind::Using> {
public:
    std::vector<std::string> members;
    OwnPtr<Expr> parent;

    UsingExpr(Span span, std::vector<std::string> members, OwnPtr<Expr> parent) :
        ExprMixin(span), members(std::move(members)), parent(std::move(parent)) {}

    Value accept(Visitor& visitor) override;
};

class TupleExpr : public ExprMixin<ExprKind::Tuple> {
public:
    ExprList<Expr> elements;

    TupleExpr(Span span, ExprList<Expr> elements) : ExprMixin(span), elements(std::move(elements)) {}
    Value accept(Visitor& visitor) override;
};

class EnumExpr : public ExprMixin<ExprKind::Enum> {
public:
    std::string name;
    OwnPtr<TypeExpr> type;
    std::vector<EnumField> fields;

    EnumExpr(Span span, std::string name, OwnPtr<TypeExpr> type, std::vector<EnumField> fields) :
        ExprMixin(span), name(std::move(name)), type(std::move(type)), fields(std::move(fields)) {}

    Value accept(Visitor& visitor) override;
};

class ImportExpr : public ExprMixin<ExprKind::Import> {
public:
    std::string name;
    bool is_wildcard;
    bool is_relative;

    ImportExpr(
        Span span, std::string name, bool is_wildcard, bool is_relative
    ) : ExprMixin(span), name(std::move(name)), is_wildcard(is_wildcard), is_relative(is_relative) {}

    Value accept(Visitor& visitor) override;
};

class ModuleExpr : public ExprMixin<ExprKind::Module> {
public:
    std::string name;
    std::vector<OwnPtr<Expr>> body;

    ModuleExpr(Span span, std::string name, std::vector<OwnPtr<Expr>> body) :
        ExprMixin(span), name(std::move(name)), body(std::move(body)) {}

    Value accept(Visitor& visitor) override;
};

class TernaryExpr : public ExprMixin<ExprKind::Ternary> {
public:
    OwnPtr<Expr> condition;
    OwnPtr<Expr> true_expr;
    OwnPtr<Expr> false_expr;

    TernaryExpr(
        Span span, OwnPtr<Expr> condition, OwnPtr<Expr> true_expr, OwnPtr<Expr> false_expr
    ) : ExprMixin(span), condition(std::move(condition)), true_expr(std::move(true_expr)), false_expr(std::move(false_expr)) {}

    Value accept(Visitor& visitor) override;
};

class ForExpr : public ExprMixin<ExprKind::For> {
public:
    Ident name;
    OwnPtr<Expr> iterable;
    OwnPtr<Expr> body;

    ForExpr(Span span, Ident name, OwnPtr<Expr> iterable, OwnPtr<Expr> body) :
        ExprMixin(span), name(std::move(name)), iterable(std::move(iterable)), body(std::move(body)) {}

    Value accept(Visitor& visitor) override;
};

class RangeForExpr : public ExprMixin<ExprKind::RangeFor> {
public:
    Ident name;
    
    OwnPtr<Expr> start;
    OwnPtr<Expr> end;
    OwnPtr<Expr> body;

    RangeForExpr(
        Span span, Ident name, OwnPtr<Expr> start, OwnPtr<Expr> end, OwnPtr<Expr> body
    ) : ExprMixin(span), name(std::move(name)), start(std::move(start)), end(std::move(end)), body(std::move(body)) {}

    Value accept(Visitor& visitor) override;
};

class TypeExpr {
public:
    Span span;
    TypeKind kind;

    TypeExpr(Span span, TypeKind kind) : span(span), kind(kind) {}

    virtual ~TypeExpr() = default;
    virtual Type* accept(Visitor& visitor) = 0;
};

class BuiltinTypeExpr : public TypeExpr {
public:
    BuiltinType value;

    BuiltinTypeExpr(Span span, BuiltinType value) : TypeExpr(span, TypeKind::Builtin), value(value) {}
    Type* accept(Visitor& visitor) override;
};

class IntegerTypeExpr : public TypeExpr {
public:
    OwnPtr<Expr> size;

    IntegerTypeExpr(Span span, OwnPtr<Expr> size) : TypeExpr(span, TypeKind::Integer), size(std::move(size)) {}
    Type* accept(Visitor& visitor) override;
};

class NamedTypeExpr : public TypeExpr {
public:
    std::string name;
    std::deque<std::string> parents;

    NamedTypeExpr(Span span, std::string name, std::deque<std::string> parents) : 
        TypeExpr(span, TypeKind::Named), name(std::move(name)), parents(std::move(parents)) {}

    Type* accept(Visitor& visitor) override;
};

class TupleTypeExpr : public TypeExpr {
public:
    ExprList<TypeExpr> types;

    TupleTypeExpr(Span span, ExprList<TypeExpr> types) : TypeExpr(span, TypeKind::Tuple), types(std::move(types)) {}
    Type* accept(Visitor& visitor) override;
};

class ArrayTypeExpr : public TypeExpr {
public:
    OwnPtr<TypeExpr> type;
    OwnPtr<Expr> size;

    ArrayTypeExpr(Span span, OwnPtr<TypeExpr> type, OwnPtr<Expr> size) :
        TypeExpr(span, TypeKind::Array), type(std::move(type)), size(std::move(size)) {}

    Type* accept(Visitor& visitor) override;
};

class PointerTypeExpr : public TypeExpr {
public:
    OwnPtr<TypeExpr> type;
    bool is_mutable;

    PointerTypeExpr(Span span, OwnPtr<TypeExpr> type, bool is_mutable) :
        TypeExpr(span, TypeKind::Pointer), type(std::move(type)), is_mutable(is_mutable) {}

    Type* accept(Visitor& visitor) override;
};

class FunctionTypeExpr : public TypeExpr {
public:
    std::vector<OwnPtr<TypeExpr>> args;
    OwnPtr<TypeExpr> ret;

    FunctionTypeExpr(Span span, std::vector<OwnPtr<TypeExpr>> args, OwnPtr<TypeExpr> ret) :
        TypeExpr(span, TypeKind::Function), args(std::move(args)), ret(std::move(ret)) {}

    Type* accept(Visitor& visitor) override;
};

class ReferenceTypeExpr : public TypeExpr {
public:
    OwnPtr<TypeExpr> type;
    bool is_mutable;

    ReferenceTypeExpr(Span span, OwnPtr<TypeExpr> type, bool is_mutable) :
        TypeExpr(span, TypeKind::Reference), type(std::move(type)), is_mutable(is_mutable) {}

    Type* accept(Visitor& visitor) override;
};

class GenericTypeExpr : public TypeExpr {
public:
    OwnPtr<NamedTypeExpr> parent;
    ExprList<TypeExpr> args;

    GenericTypeExpr(Span span, OwnPtr<NamedTypeExpr> parent, ExprList<TypeExpr> args) :
        TypeExpr(span, TypeKind::Generic), parent(std::move(parent)), args(std::move(args)) {}

    Type* accept(Visitor& visitor) override;
};

class ArrayFillExpr : public ExprMixin<ExprKind::ArrayFill> {
public:
    OwnPtr<Expr> element;
    OwnPtr<Expr> count;

    ArrayFillExpr(Span span, OwnPtr<Expr> element, OwnPtr<Expr> count) :
        ExprMixin(span), element(std::move(element)), count(std::move(count)) {}

    Value accept(Visitor& visitor) override;
};

class TypeAliasExpr : public ExprMixin<ExprKind::TypeAlias> {
public:
    std::string name;
    OwnPtr<TypeExpr> type;

    std::vector<GenericParameter> parameters;

    TypeAliasExpr(
        Span span, std::string name, OwnPtr<TypeExpr> type, std::vector<GenericParameter> parameters
    ) : ExprMixin(span), name(std::move(name)), type(std::move(type)), parameters(std::move(parameters)) {}

    Value accept(Visitor& visitor) override;

    [[nodiscard]] bool is_generic_alias() const { return !this->parameters.empty(); }
};

class StaticAssertExpr : public ExprMixin<ExprKind::StaticAssert> {
public:
    OwnPtr<Expr> condition;
    std::string message;

    StaticAssertExpr(Span span, OwnPtr<Expr> condition, std::string message)
        : ExprMixin(span), condition(std::move(condition)), message(std::move(message)) {}

    Value accept(Visitor& visitor) override;
};

class MaybeExpr : public ExprMixin<ExprKind::Maybe> {
public:
    OwnPtr<Expr> value;

    MaybeExpr(Span span, OwnPtr<Expr> value) : ExprMixin(span), value(std::move(value)) {}

    Value accept(Visitor& visitor) override;
};

class ImplExpr : public ExprMixin<ExprKind::Impl> {
public:
    OwnPtr<TypeExpr> type;
    ExprList<FunctionExpr> body;

    ImplExpr(Span span, OwnPtr<TypeExpr> type, ExprList<FunctionExpr> body) :
        ExprMixin(span), type(std::move(type)), body(std::move(body)) {}

    Value accept(Visitor& visitor) override;
};

class MatchExpr : public ExprMixin<ExprKind::Match> {
public:
    OwnPtr<Expr> value;
    std::vector<MatchArm> arms;

    MatchExpr(Span span, OwnPtr<Expr> value, std::vector<MatchArm> arms) :
        ExprMixin(span), value(std::move(value)), arms(std::move(arms)) {}

    Value accept(Visitor& visitor) override;
};

};

}