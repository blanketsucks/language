#pragma once

#include <quart/lexer/tokens.h>
#include <quart/attributes/attributes.h>

#include <quart/errors.h>
#include <quart/bytecode/register.h>

#include <quart/language/functions.h>

#include <memory>
#include <vector>
#include <deque>

#define ENUMERATE_EXPR_KINDS(Op)     \
    Op(Block)                        \
    Op(ExternBlock)                  \
    Op(Integer)                      \
    Op(Float)                        \
    Op(String)                       \
    Op(Identifier)                   \
    Op(Assignment)                   \
    Op(TupleAssignment)              \
    Op(Const)                        \
    Op(Array)                        \
    Op(UnaryOp)                      \
    Op(Reference)                    \
    Op(BinaryOp)                     \
    Op(InplaceBinaryOp)              \
    Op(Call)                         \
    Op(Return)                       \
    Op(FunctionDecl)                 \
    Op(Function)                     \
    Op(Defer)                        \
    Op(If)                           \
    Op(While)                        \
    Op(For)                          \
    Op(Break)                        \
    Op(Continue)                     \
    Op(Struct)                       \
    Op(Constructor)                  \
    Op(EmptyConstructor)             \
    Op(Attribute)                    \
    Op(Index)                        \
    Op(Cast)                         \
    Op(Sizeof)                       \
    Op(Offsetof)                     \
    Op(Path)                         \
    Op(Using)                        \
    Op(Tuple)                        \
    Op(Enum)                         \
    Op(Import)                       \
    Op(Ternary)                      \
    Op(ArrayFill)                    \
    Op(TypeAlias)                    \
    Op(StaticAssert)                 \
    Op(Maybe)                        \
    Op(Module)                       \
    Op(Impl)                         \
    Op(Trait)                        \
    Op(ImplTrait)                    \
    Op(Match)                        \
    Op(RangeFor)                     \
    Op(Bool)

namespace quart {

namespace ast {
    class TypeExpr;
}

class BytecodeResult : public ErrorOr<Optional<bytecode::Register>> {
public:
    BytecodeResult() = default;

    BytecodeResult(Error error) : ErrorOr<Optional<bytecode::Register>>(move(error)) {}

    BytecodeResult(bytecode::Register value) : ErrorOr<Optional<bytecode::Register>>(value) {}
    BytecodeResult(Optional<bytecode::Register> value) : ErrorOr<Optional<bytecode::Register>>(value) {}
};

class State;
class Type;

struct PathSegment {
public:
    PathSegment() = default;
    PathSegment(String name, Vector<OwnPtr<ast::TypeExpr>> arguments) : m_name(move(name)), m_arguments(move(arguments)) {}

    String const& name() const { return m_name; }
    Vector<OwnPtr<ast::TypeExpr>> const& arguments() const { return m_arguments; }

    bool has_generic_arguments() const { return !m_arguments.empty(); }

private:
    String m_name;
    Vector<OwnPtr<ast::TypeExpr>> m_arguments;
};

struct Path {
public:
    Path() = default;
    Path(PathSegment last, Deque<PathSegment> segments) : m_last(move(last)), m_segments(move(segments)) {}

    PathSegment const& last() const { return m_last; }
    Deque<PathSegment> const& segments() const { return m_segments; }

    String const& name() const { return m_last.name(); }

    String format() const;
    void rearrange();

    void push(PathSegment segment) {
        m_segments.push_back(move(segment));
    }

private:
    PathSegment m_last;
    std::deque<PathSegment> m_segments;
};

namespace ast {

class Expr;

template<class T = Expr> using ExprList = Vector<OwnPtr<T>>;

enum class ExprKind : u8 {
    Block,
    ExternBlock,
    Integer,
    Float,
    String,
    Identifier,
    Assignment,
    TupleAssignment,
    Const,
    Array,
    UnaryOp,
    Reference,
    BinaryOp,
    InplaceBinaryOp,
    Call,
    Return,
    FunctionDecl,
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
    Path,
    Using,
    Tuple,
    Enum,
    Import,
    Ternary,
    ArrayFill,
    TypeAlias,
    StaticAssert,
    Maybe,
    Module,
    Impl,
    Trait,
    ImplTrait,
    Match,
    RangeFor,
    Bool,
    ConstEval,
};

enum class TypeKind : u8 {
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

enum class BuiltinType : u8 {
    None = 0,
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
    usize,
    isize,
    Void
};

struct IntegerSuffix {
    BuiltinType type = BuiltinType::None;
};

class Attributes {
public:
    using HashMapType = HashMap<Attribute::Type, Attribute>;
    using ConstIterator = HashMapType::const_iterator;

    HashMap<Attribute::Type, Attribute> const& value() const {
        return m_values;
    }

    void insert(Attribute attribute) {
        m_values[attribute.type()] = move(attribute);
    }

    void insert(Attributes other) {
        m_values.insert(other.m_values.begin(), other.m_values.end());
    }

    [[nodiscard]] bool has(Attribute::Type type) const {
        return m_values.contains(type);
    }

    [[nodiscard]] Attribute const& get(Attribute::Type type) const {
        return m_values.at(type);
    }

    Attribute const& operator[](Attribute::Type type) const {
        return m_values.at(type);
    }

    ConstIterator begin() const {
        return m_values.begin();
    }

    ConstIterator end() const {
        return m_values.end();
    }

private:
    HashMap<Attribute::Type, Attribute> m_values;
};

struct Ident {
    String value;
    bool is_mutable;

    Span span;
};

struct Parameter {
    String name;

    OwnPtr<TypeExpr> type;
    OwnPtr<Expr> default_value;

    u8 flags;

    Span span;
};

struct FunctionParameters {
    Vector<Parameter> parameters;
    bool is_c_variadic;
};

struct StructField {
    String name;
    OwnPtr<TypeExpr> type;

    u32 index;
    u8 flags;
};

struct ConstructorArgument {
    String name;
    OwnPtr<Expr> value;

    Span span;
};

struct EnumField {
    String name;
    OwnPtr<Expr> value = nullptr;
};

struct GenericParameter {
    String name;

    ExprList<TypeExpr> constraints;
    OwnPtr<TypeExpr> default_type;

    Span span;
};

struct MatchPattern {
    bool is_wildcard = false;
    bool is_conditional = false;

    Vector<OwnPtr<Expr>> values; // A | B | C
    Span span;
};

struct MatchArm {
    MatchPattern pattern;
    OwnPtr<Expr> body;

    size_t index;
    
    bool is_wildcard() const { return this->pattern.is_wildcard; }
};

class Expr {
public:
    NO_COPY(Expr)
    DEFAULT_MOVE(Expr)

    Expr(Span span, ExprKind kind) : m_span(span), m_kind(kind) {}
    virtual ~Expr() = default;

    ExprKind kind() const { return m_kind; }
    Span span() const { return m_span; }

    Attributes& attributes() { return m_attrs; }
    const Attributes& attributes() const { return m_attrs; }

    bool is(ExprKind kind) const { return m_kind == kind; }

    template<typename... Args> requires(of_type_v<ExprKind, Args...>)
    bool is(ExprKind kind, Args... args) const {
        return m_kind == kind || this->is(args...);
    }

    template<typename T> requires(std::is_base_of_v<Expr, T>)
    bool is() const {
        return T::classof(this);
    }

    template<typename T> requires(std::is_base_of_v<Expr, T>)
    T const* as() const {
        return T::classof(this) ? static_cast<T const*>(this) : nullptr;
    }

    bool is_function_decl() const { return this->is(ExprKind::FunctionDecl); }

    virtual BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const = 0;

protected:
    Attributes m_attrs; // NOLINT

private:
    Span m_span;
    ExprKind m_kind;
};

template<ExprKind Kind>
class ExprBase : public Expr {
public:
    static bool classof(const Expr* expr) { return expr->kind() == Kind; }

    ExprBase(Span span) : Expr(span, Kind) {}
};

class BlockExpr : public ExprBase<ExprKind::Block> {
public:
    BlockExpr(Span span, ExprList<Expr> block) : ExprBase(span), m_block(move(block)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    const ExprList<Expr>& block() const { return m_block; }
    
private:
    ExprList<Expr> m_block;
};

class ExternBlockExpr : public ExprBase<ExprKind::ExternBlock> {
public:
    ExternBlockExpr(Span span, ExprList<Expr> block) : ExprBase(span), m_block(move(block)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    const ExprList<Expr>& block() const { return m_block; }
    
private:
    ExprList<Expr> m_block;
};

class IntegerExpr : public ExprBase<ExprKind::Integer> {
public:
    IntegerExpr(Span span, u64 value, IntegerSuffix suffix) : ExprBase(span), m_value(value), m_suffix(suffix) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    u64 value() const { return m_value; }
    IntegerSuffix suffix() const { return m_suffix; }

private:
    u64 m_value;
    IntegerSuffix m_suffix;
};

class FloatExpr : public ExprBase<ExprKind::Float> {
public:
    FloatExpr(Span span, double value, bool is_double) : ExprBase(span), m_value(value), m_is_double(is_double) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    double value() const { return m_value; }
    bool is_double() const { return m_is_double; }

private:
    double m_value;
    bool m_is_double;
};

class StringExpr : public ExprBase<ExprKind::String> {
public:
    StringExpr(Span span, String value) : ExprBase(span), m_value(move(value)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& value() const { return m_value; }

private:
    String m_value;
};

class IdentifierExpr : public ExprBase<ExprKind::Identifier> {
public:
    IdentifierExpr(Span span, String name) : ExprBase(span), m_name(move(name)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }

private:
    String m_name;
};

class AssignmentExpr : public ExprBase<ExprKind::Assignment> {
public:
    AssignmentExpr(
        Span span,
        Ident identifier,
        OwnPtr<TypeExpr> type, 
        OwnPtr<Expr> value,
        bool is_public
    ) : ExprBase(span), m_identifier(move(identifier)), m_type(move(type)), m_value(move(value)), m_is_public(is_public) {}
    
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    const Ident& identifier() const { return m_identifier; }

    const TypeExpr* type() const { return m_type.get(); }
    const Expr* value() const { return m_value.get(); }

    bool is_public() const { return m_is_public; }

private:
    Ident m_identifier;
    OwnPtr<TypeExpr> m_type;
    OwnPtr<Expr> m_value;

    bool m_is_public;
};

class TupleAssignmentExpr : public ExprBase<ExprKind::TupleAssignment> {
public:
    TupleAssignmentExpr(
        Span span, 
        Vector<Ident> identifiers, 
        OwnPtr<TypeExpr> type, 
        OwnPtr<Expr> value
    ) : ExprBase(span), m_identifiers(move(identifiers)), m_type(move(type)), m_value(move(value)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    const Vector<Ident>& identifiers() const { return m_identifiers; }

    const TypeExpr* type() const { return m_type.get(); }
    const Expr* value() const { return m_value.get(); }

private:
    Vector<Ident> m_identifiers;
    OwnPtr<TypeExpr> m_type;
    OwnPtr<Expr> m_value;
};

class ConstExpr : public ExprBase<ExprKind::Const> {
public:
    ConstExpr(
        Span span, String name, OwnPtr<TypeExpr> type, OwnPtr<Expr> value, bool is_public
    ) : ExprBase(span), m_name(move(name)), m_type(move(type)), m_value(move(value)), m_is_public(is_public) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }

    const TypeExpr* type() const { return m_type.get(); }
    Expr const& value() const { return *m_value; }

    bool is_public() const { return m_is_public; }

private:
    String m_name;
    OwnPtr<TypeExpr> m_type;
    OwnPtr<Expr> m_value;

    bool m_is_public;
};

class ArrayExpr : public ExprBase<ExprKind::Array> {
public:
    ArrayExpr(Span span, ExprList<Expr> elements) : ExprBase(span), m_elements(move(elements)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    const ExprList<Expr>& elements() const { return m_elements; }

private:
    ExprList<Expr> m_elements;
};

class UnaryOpExpr : public ExprBase<ExprKind::UnaryOp> {
public:
    UnaryOpExpr(Span span, OwnPtr<Expr> value, UnaryOp op) : ExprBase(span), m_value(move(value)), m_op(op) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }

    UnaryOp op() const { return m_op; }

private:
    OwnPtr<Expr> m_value;
    UnaryOp m_op;
};

class ReferenceExpr : public ExprBase<ExprKind::Reference> {
public:
    ReferenceExpr(
        Span span, OwnPtr<Expr> value, bool is_mutable
    ) : ExprBase(span), m_value(move(value)), m_is_mutable(is_mutable) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }
    bool is_mutable() const { return m_is_mutable; }

private:
    OwnPtr<Expr> m_value;
    bool m_is_mutable;
};

class BinaryOpExpr : public ExprBase<ExprKind::BinaryOp> {
public:
    BinaryOpExpr(
        Span span, BinaryOp op, OwnPtr<Expr> lhs, OwnPtr<Expr> rhs
    ) : ExprBase(span), m_lhs(move(lhs)), m_rhs(move(rhs)), m_op(op) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& lhs() const { return *m_lhs; }
    Expr const& rhs() const { return *m_rhs; }

    BinaryOp op() const { return m_op; }

private:
    OwnPtr<Expr> m_lhs;
    OwnPtr<Expr> m_rhs;
    BinaryOp m_op;
};

class InplaceBinaryOpExpr : public ExprBase<ExprKind::InplaceBinaryOp> {
public:
    InplaceBinaryOpExpr(
        Span span, BinaryOp op, OwnPtr<Expr> lhs, OwnPtr<Expr> rhs
    ) : ExprBase(span), m_lhs(move(lhs)), m_rhs(move(rhs)), m_op(op) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& lhs() const { return *m_lhs; }
    Expr const& rhs() const { return *m_rhs; }

    BinaryOp op() const { return m_op; }

private:
    OwnPtr<Expr> m_lhs;
    OwnPtr<Expr> m_rhs;
    BinaryOp m_op;
};

class CallExpr : public ExprBase<ExprKind::Call> {
public:
    CallExpr(
        Span span, 
        OwnPtr<ast::Expr> callee, 
        ExprList<Expr> args,
        HashMap<String, OwnPtr<Expr>> kwargs
    ) : ExprBase(span), m_callee(move(callee)), m_args(move(args)), m_kwargs(move(kwargs)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& callee() const { return *m_callee; }

    const ExprList<Expr>& args() const { return m_args; }
    const HashMap<String, OwnPtr<Expr>>& kwargs() const { return m_kwargs; }

private:
    OwnPtr<Expr> m_callee;

    ExprList<Expr> m_args;
    HashMap<String, OwnPtr<Expr>> m_kwargs;
};

class ReturnExpr : public ExprBase<ExprKind::Return> {
public:
    ReturnExpr(Span span, OwnPtr<Expr> value) : ExprBase(span), m_value(move(value)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }

private:
    OwnPtr<Expr> m_value;
};

class FunctionDeclExpr : public ExprBase<ExprKind::FunctionDecl> {
public:
    FunctionDeclExpr(
        Span span,
        String name,
        Vector<Parameter> parameters,
        OwnPtr<TypeExpr> return_type,
        LinkageSpecifier linkage,
        bool is_c_variadic,
        bool is_public
    ) : ExprBase(span), m_name(move(name)), m_parameters(move(parameters)), m_return_type(move(return_type)),
        m_is_c_variadic(is_c_variadic), m_is_public(is_public), m_linkage(linkage) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }
    const Vector<Parameter>& parameters() const { return m_parameters; }

    const TypeExpr* return_type() const { return m_return_type.get(); }

    bool is_c_variadic() const { return m_is_c_variadic; }
    bool is_public() const { return m_is_public; }

    LinkageSpecifier linkage() const { return m_linkage; }

private:
    String m_name;
    Vector<Parameter> m_parameters;
    OwnPtr<TypeExpr> m_return_type;

    bool m_is_c_variadic = false;
    bool m_is_public = false;

    LinkageSpecifier m_linkage;
};

class FunctionExpr : public ExprBase<ExprKind::Function> {
public:
    FunctionExpr(
        Span span, OwnPtr<FunctionDeclExpr> decl, Vector<OwnPtr<Expr>> body
    ) : ExprBase(span), m_decl(move(decl)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    const FunctionDeclExpr& decl() const { return *m_decl; }
    const Vector<OwnPtr<Expr>>& body() const { return m_body; }

private:
    OwnPtr<FunctionDeclExpr> m_decl;
    Vector<OwnPtr<Expr>> m_body;
};

class DeferExpr : public ExprBase<ExprKind::Defer> {
public:
    DeferExpr(Span span, OwnPtr<Expr> expr) : ExprBase(span), m_expr(move(expr)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& expr() const { return *m_expr; }

public:
    OwnPtr<Expr> m_expr;
};

class IfExpr : public ExprBase<ExprKind::If> {
public:
    IfExpr(
        Span span, OwnPtr<Expr> condition, OwnPtr<Expr> body, OwnPtr<Expr> else_body
    ) : ExprBase(span), m_condition(move(condition)), m_body(move(body)), m_else_body(move(else_body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& condition() const { return *m_condition; }
    Expr const& body() const { return *m_body; }
    const Expr* else_body() const { return m_else_body.get(); }

private:
    OwnPtr<Expr> m_condition;
    OwnPtr<Expr> m_body;
    OwnPtr<Expr> m_else_body;
};

class WhileExpr : public ExprBase<ExprKind::While> {
public:
    WhileExpr(
        Span span, OwnPtr<Expr> condition, OwnPtr<BlockExpr> body
    ) : ExprBase(span), m_condition(move(condition)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& condition() const { return *m_condition; }
    const BlockExpr& body() const { return *m_body; }

private:
    OwnPtr<Expr> m_condition;
    OwnPtr<BlockExpr> m_body;
};

class BreakExpr : public ExprBase<ExprKind::Break> {
public:
    BreakExpr(Span span) : ExprBase(span) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;  
};

class ContinueExpr : public ExprBase<ExprKind::Continue> {
public:
    ContinueExpr(Span span) : ExprBase(span) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;
};

class StructExpr : public ExprBase<ExprKind::Struct> {
public:
    StructExpr(
        Span span, 
        String name,
        bool opaque,
        Vector<GenericParameter> parameters,
        Vector<StructField> fields, 
        Vector<OwnPtr<Expr>> members,
        bool is_public
    ) : ExprBase(span), m_name(move(name)), m_opaque(opaque), m_parameters(move(parameters)), m_fields(move(fields)), m_members(move(members)), m_is_public(is_public) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }

    bool is_opaque() const { return m_opaque; }
    bool is_public() const { return m_is_public; }

    const Vector<GenericParameter>& parameters() const { return m_parameters; }
    const Vector<StructField>& fields() const { return m_fields; }
    const Vector<OwnPtr<Expr>>& members() const { return m_members; }

private:
    String m_name;
    bool m_opaque;
    
    Vector<GenericParameter> m_parameters;
    Vector<StructField> m_fields;
    Vector<OwnPtr<Expr>> m_members;

    bool m_is_public;
};

class ConstructorExpr : public ExprBase<ExprKind::Constructor> {
public:
    ConstructorExpr(Span span, OwnPtr<Expr> parent, Vector<ConstructorArgument> arguments) :
        ExprBase(span), m_parent(move(parent)), m_arguments(move(arguments)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& parent() const { return *m_parent; }
    Vector<ConstructorArgument> const& arguments() const { return m_arguments; }

private:
    OwnPtr<Expr> m_parent;
    Vector<ConstructorArgument> m_arguments;
};

class EmptyConstructorExpr : public ExprBase<ExprKind::EmptyConstructor> {
public:
    EmptyConstructorExpr(Span span, OwnPtr<Expr> parent) : ExprBase(span), m_parent(move(parent)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& parent() const { return *m_parent; }

private:
    OwnPtr<Expr> m_parent;
};

class AttributeExpr : public ExprBase<ExprKind::Attribute> {
public:
    AttributeExpr(
        Span span, OwnPtr<Expr> parent, String attribute
    ) : ExprBase(span), m_parent(move(parent)), m_attribute(move(attribute)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& parent() const { return *m_parent; }
    String const& attribute() const { return m_attribute; }

private:
    OwnPtr<Expr> m_parent;
    String m_attribute;
};

class IndexExpr : public ExprBase<ExprKind::Index> {
public:
    IndexExpr(
        Span span, OwnPtr<Expr> value, OwnPtr<Expr> index
    ) : ExprBase(span), m_value(move(value)), m_index(move(index)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }
    Expr const& index() const { return *m_index; }

private:
    OwnPtr<Expr> m_value;
    OwnPtr<Expr> m_index;
};

class CastExpr : public ExprBase<ExprKind::Cast> {
public:
    CastExpr(
        Span span, OwnPtr<Expr> value, OwnPtr<TypeExpr> to
    ) : ExprBase(span), m_value(move(value)), m_to(move(to)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }
    const TypeExpr& to() const { return *m_to; }

private:
    OwnPtr<Expr> m_value;
    OwnPtr<TypeExpr> m_to;
};

class SizeofExpr : public ExprBase<ExprKind::Sizeof> {
public:
    SizeofExpr(Span span, OwnPtr<Expr> value) : ExprBase(span), m_value(move(value)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }

private:
    OwnPtr<Expr> m_value;
};

class OffsetofExpr : public ExprBase<ExprKind::Offsetof> {
public:
    OffsetofExpr(Span span, OwnPtr<Expr> value, String field) : ExprBase(span), m_value(move(value)), m_field(move(field)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }
    String const& field() const { return m_field; }

private:
    OwnPtr<Expr> m_value;
    String m_field;
};

class PathExpr : public ExprBase<ExprKind::Path> {
public:
    PathExpr(Span span, Path path) : ExprBase(span), m_path(move(path)) {}
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Path const& path() const { return m_path; }

private:
    Path m_path;
};

class UsingExpr : public ExprBase<ExprKind::Using> {
public:
    UsingExpr(Span span, Path path, Vector<String> symbols) : ExprBase(span), m_path(move(path)), m_symbols(move(symbols)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Path const& path() const { return m_path; }
    Vector<String> const& symbols() const { return m_symbols; }

private:
    Path m_path;
    Vector<String> m_symbols;
};

class TupleExpr : public ExprBase<ExprKind::Tuple> {
public:
    TupleExpr(Span span, ExprList<Expr> elements) : ExprBase(span), m_elements(move(elements)) {}
    
    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    ExprList<Expr> const& elements() const { return m_elements; }

private:
    ExprList<Expr> m_elements;
};

class EnumExpr : public ExprBase<ExprKind::Enum> {
public:
    EnumExpr(
        Span span, String name, OwnPtr<TypeExpr> type, Vector<EnumField> fields
    ) : ExprBase(span), m_name(move(name)), m_type(move(type)), m_fields(move(fields)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }
    TypeExpr const& type() const { return *m_type; }

    Vector<EnumField> const& fields() const { return m_fields; }

private:
    String m_name;
    OwnPtr<TypeExpr> m_type;
    Vector<EnumField> m_fields;
};

class ImportExpr : public ExprBase<ExprKind::Import> {
public:
    ImportExpr(
        Span span, Path path, bool is_wildcard, bool is_relative, Vector<String> symbols
    ) : ExprBase(span), m_path(move(path)), m_is_wildcard(is_wildcard), m_is_relative(is_relative), m_symbols(move(symbols)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Path const& path() const { return m_path; }
    
    bool is_wildcard() const { return m_is_wildcard; }
    bool is_relative() const { return m_is_relative; }

    Vector<String> const& symbols() const { return m_symbols; }

private:
    Path m_path;

    bool m_is_wildcard;
    bool m_is_relative;

    Vector<String> m_symbols;
};

class ModuleExpr : public ExprBase<ExprKind::Module> {
public:
    ModuleExpr(Span span, String name, ExprList<Expr> body) : ExprBase(span), m_name(move(name)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }
    ExprList<Expr> const& body() const { return m_body; }

private:
    String m_name;
    ExprList<Expr> m_body;
};

class TernaryExpr : public ExprBase<ExprKind::Ternary> {
public:
    TernaryExpr(
        Span span, OwnPtr<Expr> condition, OwnPtr<Expr> true_expr, OwnPtr<Expr> false_expr
    ) : ExprBase(span), m_condition(move(condition)), m_true_expr(move(true_expr)), m_false_expr(move(false_expr)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& condition() const { return *m_condition; }
    Expr const& true_expr() const { return *m_true_expr; }
    Expr const& false_expr() const { return *m_false_expr; }

private:
    OwnPtr<Expr> m_condition;
    OwnPtr<Expr> m_true_expr;
    OwnPtr<Expr> m_false_expr;
};

class ForExpr : public ExprBase<ExprKind::For> {
public:
    ForExpr(
        Span span, Ident identifier, OwnPtr<Expr> iterable, OwnPtr<Expr> body
    ) : ExprBase(span), m_identifier(move(identifier)), m_iterable(move(iterable)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Ident const& identifier() const { return m_identifier; }

    Expr const& iterable() const { return *m_iterable; }
    Expr const& body() const { return *m_body; }

private:
    Ident m_identifier;
    OwnPtr<Expr> m_iterable;
    OwnPtr<Expr> m_body;
};

class RangeForExpr : public ExprBase<ExprKind::RangeFor> {
public:
    RangeForExpr(
        Span span, Ident identifier, bool inclusive, OwnPtr<Expr> start, OwnPtr<Expr> end, OwnPtr<Expr> body
    ) : ExprBase(span), m_identifier(move(identifier)), m_inclusive(inclusive), m_start(move(start)), m_end(move(end)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Ident const& identifier() const { return m_identifier; }
    bool inclusive() const { return m_inclusive; }

    Expr const& start() const { return *m_start; }
    Expr const& end() const { return *m_end; }

    Expr const& body() const { return *m_body; }

private:
    Ident m_identifier;
    bool m_inclusive;

    OwnPtr<Expr> m_start;
    OwnPtr<Expr> m_end;
    OwnPtr<Expr> m_body;

};

class ArrayFillExpr : public ExprBase<ExprKind::ArrayFill> {
public:
    ArrayFillExpr(Span span, OwnPtr<Expr> value, OwnPtr<Expr> count) : ExprBase(span), m_value(move(value)), m_count(move(count)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }
    Expr const& count() const { return *m_count; }

private:
    OwnPtr<Expr> m_value;
    OwnPtr<Expr> m_count;
};

class TypeAliasExpr : public ExprBase<ExprKind::TypeAlias> {
public:
    TypeAliasExpr(
        Span span, String name, OwnPtr<TypeExpr> type, Vector<GenericParameter> parameters, bool is_public
    ) : ExprBase(span), m_name(move(name)), m_type(move(type)), m_parameters(move(parameters)), m_is_public(is_public) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }
    Vector<GenericParameter> const& parameters() const { return m_parameters; }

    TypeExpr const& type() const { return *m_type; }

    bool is_public() const { return m_is_public; }

private:
    String m_name;
    OwnPtr<TypeExpr> m_type;

    Vector<GenericParameter> m_parameters;

    bool m_is_public = false;
};

class StaticAssertExpr : public ExprBase<ExprKind::StaticAssert> {
public:
    StaticAssertExpr(Span span, OwnPtr<Expr> condition, String message) : ExprBase(span), m_condition(move(condition)), m_message(move(message)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& condition() const { return *m_condition; }
    String const& message() const { return m_message; }

private:
    OwnPtr<Expr> m_condition;
    String m_message;
};

class MaybeExpr : public ExprBase<ExprKind::Maybe> {
public:
    MaybeExpr(Span span, OwnPtr<Expr> value) : ExprBase(span), m_value(move(value)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }

private:
    OwnPtr<Expr> m_value;
};

class ImplExpr : public ExprBase<ExprKind::Impl> {
public:
    ImplExpr(
        Span span, OwnPtr<TypeExpr> type, OwnPtr<BlockExpr> body, Vector<GenericParameter> parameters
    ) : ExprBase(span), m_type(move(type)), m_body(move(body)), m_parameters(move(parameters)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    TypeExpr const& type() const { return *m_type; }
    BlockExpr const& body() const { return *m_body; }
    Vector<GenericParameter> const& parameters() const { return m_parameters; }

private:
    OwnPtr<TypeExpr> m_type;
    OwnPtr<BlockExpr> m_body;
    
    Vector<GenericParameter> m_parameters;
};

class TraitExpr : public ExprBase<ExprKind::Trait> {
public:
    TraitExpr(Span span, String name, ExprList<> body) : ExprBase(span), m_name(move(name)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    String const& name() const { return m_name; }
    ExprList<> const& body() const { return m_body; }

private:
    String m_name;
    ExprList<> m_body;
};

class ImplTraitExpr : public ExprBase<ExprKind::ImplTrait> {
public:
    ImplTraitExpr(Span span, OwnPtr<TypeExpr> trait, OwnPtr<TypeExpr> type, ExprList<> body) : ExprBase(span), m_trait(move(trait)), m_type(move(type)), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    TypeExpr const& trait() const { return *m_trait; }
    TypeExpr const& type() const { return *m_type; }
    ExprList<> const& body() const { return m_body; }

private:
    OwnPtr<TypeExpr> m_trait;
    OwnPtr<TypeExpr> m_type;
    ExprList<> m_body;
};

class MatchExpr : public ExprBase<ExprKind::Match> {
public:
    MatchExpr(Span span, OwnPtr<Expr> value, Vector<MatchArm> arms) : ExprBase(span), m_value(move(value)), m_arms(move(arms)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Expr const& value() const { return *m_value; }
    Vector<MatchArm> const& arms() const { return m_arms; }

private:
    OwnPtr<Expr> m_value;
    Vector<MatchArm> m_arms;
};

class BoolExpr : public ExprBase<ExprKind::Bool> {
public:
    enum Value {
        False,
        True,
        Null, // Not really a "boolean", but i don't know where else to put it
    };

    BoolExpr(Span span, Value value) : ExprBase(span), m_value(value) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    Value value() const { return m_value; }

private:
    Value m_value;
};

class ConstEvalExpr : public ExprBase<ExprKind::Bool> {
public:
    ConstEvalExpr(Span span, ExprList<> body) : ExprBase(span), m_body(move(body)) {}

    BytecodeResult generate(State&, Optional<bytecode::Register> dst = {}) const override;

    ExprList<> const& body() const { return m_body; }

private:
    ExprList<> m_body;
};

class TypeExpr {
public:
    NO_COPY(TypeExpr)
    DEFAULT_MOVE(TypeExpr)

    TypeExpr(Span span, TypeKind kind) : m_span(span), m_kind(kind) {}
    virtual ~TypeExpr() = default;

    TypeKind kind() const { return m_kind; }
    Span span() const { return m_span; }

    template<typename T> requires(std::is_base_of_v<TypeExpr, T>)
    bool is() const {
        return T::classof(this);
    }

    template<typename T> requires(std::is_base_of_v<TypeExpr, T>)
    T const* as() const {
        return T::classof(this) ? static_cast<T const*>(this) : nullptr;
    }

    virtual ErrorOr<Type*> evaluate(State&) = 0;

private:
    Span m_span;
    TypeKind m_kind;
};

template<TypeKind Kind>
class TypeExprBase : public TypeExpr {
public:
    static bool classof(TypeExpr const* expr) { return expr->kind() == Kind; }

    TypeExprBase(Span span) : TypeExpr(span, Kind) {}
};

class BuiltinTypeExpr : public TypeExprBase<TypeKind::Builtin> {
public:
    BuiltinTypeExpr(Span span, BuiltinType value) : TypeExprBase(span), m_value(value) {}
    ErrorOr<Type*> evaluate(State&) override;

    BuiltinType value() const { return m_value; }

private:
    BuiltinType m_value;
};

class IntegerTypeExpr : public TypeExprBase<TypeKind::Integer> {
public:
    IntegerTypeExpr(Span span, OwnPtr<Expr> size) : TypeExprBase(span), m_size(move(size)) {}
    ErrorOr<Type*> evaluate(State&) override;

    Expr const& size() const { return *m_size; }

private:
    OwnPtr<Expr> m_size;
};

class NamedTypeExpr : public TypeExprBase<TypeKind::Named> {
public:
    NamedTypeExpr(Span span, Path path) : TypeExprBase(span), m_path(move(path)) {}
    ErrorOr<Type*> evaluate(State&) override;

    Path const& path() const { return m_path; }
    
private:
    Path m_path;
};

class TupleTypeExpr : public TypeExprBase<TypeKind::Tuple> {
public:
    TupleTypeExpr(Span span, ExprList<TypeExpr> types) : TypeExprBase(span), m_types(move(types)) {}
    ErrorOr<Type*> evaluate(State&) override;

    ExprList<TypeExpr> const& types() const { return m_types; }

private:
    ExprList<TypeExpr> m_types;
};

class ArrayTypeExpr : public TypeExprBase<TypeKind::Array> {
public:
    ArrayTypeExpr(Span span, OwnPtr<TypeExpr> type, OwnPtr<Expr> size) : TypeExprBase(span), m_type(move(type)), m_size(move(size)) {}
    ErrorOr<Type*> evaluate(State&) override;

    TypeExpr const& type() const { return *m_type; }
    Expr const& size() const { return *m_size; }

private:
    OwnPtr<TypeExpr> m_type;
    OwnPtr<Expr> m_size;
};

class PointerTypeExpr : public TypeExprBase<TypeKind::Pointer> {
public:
    PointerTypeExpr(Span span, OwnPtr<TypeExpr> pointee, bool is_mutable) : TypeExprBase(span), m_pointee(move(pointee)), m_is_mutable(is_mutable) {}
    ErrorOr<Type*> evaluate(State&) override;

    TypeExpr const& pointee() const { return *m_pointee; }
    bool is_mutable() const { return m_is_mutable; }

private:
    OwnPtr<TypeExpr> m_pointee;
    bool m_is_mutable;
};

class ReferenceTypeExpr : public TypeExprBase<TypeKind::Reference> {
public:
    ReferenceTypeExpr(Span span, OwnPtr<TypeExpr> type, bool is_mutable) : TypeExprBase(span), m_type(move(type)), m_is_mutable(is_mutable) {}
    ErrorOr<Type*> evaluate(State&) override;

    TypeExpr const& type() const { return *m_type; }
    bool is_mutable() const { return m_is_mutable; }

private:
    OwnPtr<TypeExpr> m_type;
    bool m_is_mutable;
};

class FunctionTypeExpr : public TypeExprBase<TypeKind::Function> {
public:
    FunctionTypeExpr(
        Span span, ExprList<TypeExpr> parameters, OwnPtr<TypeExpr> return_type
    ) : TypeExprBase(span), m_parameters(move(parameters)), m_return_type(move(return_type)) {}

    ErrorOr<Type*> evaluate(State&) override;

    ExprList<TypeExpr> const& parameters() const { return m_parameters; }
    TypeExpr const& return_type() const { return *m_return_type; }

private:
    ExprList<TypeExpr> m_parameters;
    OwnPtr<TypeExpr> m_return_type;
};

class GenericTypeExpr : public TypeExprBase<TypeKind::Generic> {
public:
    GenericTypeExpr(
        Span span, OwnPtr<NamedTypeExpr> parent, ExprList<TypeExpr> args
    ) : TypeExprBase(span), m_parent(move(parent)), m_args(move(args)) {}

    ErrorOr<Type*> evaluate(State&) override;

    NamedTypeExpr const& parent() const { return *m_parent; }
    ExprList<TypeExpr> const& args() const { return m_args; }

private:
    OwnPtr<NamedTypeExpr> m_parent;
    ExprList<TypeExpr> m_args;
};

};

}

namespace llvm {

template<>
struct format_provider<quart::PathSegment> {
    static void format(const quart::PathSegment& segment, raw_ostream& stream, StringRef) {
        stream << segment.name();
    }
};

}