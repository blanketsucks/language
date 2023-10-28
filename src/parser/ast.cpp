#include <quart/parser/ast.h>
#include <quart/visitor.h>

#include <vector>

using namespace quart::ast;

BlockExpr::BlockExpr(
    Span span, std::vector<std::unique_ptr<Expr>> block
) : ExprMixin(span) {
    this->block = std::move(block);
}

quart::Value BlockExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ExternBlockExpr::ExternBlockExpr(
    Span span, std::vector<std::unique_ptr<Expr>> block
) : ExprMixin(span) {
    this->block = std::move(block);
}

quart::Value ExternBlockExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IntegerExpr::IntegerExpr(
    Span span, std::string value, int bits, bool is_float
) : ExprMixin(span), value(value), bits(bits), is_float(is_float) {}

quart::Value IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CharExpr::CharExpr(Span span, char value) : ExprMixin(span), value(value) {}

quart::Value CharExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FloatExpr::FloatExpr(
    Span span, double value, bool is_double
) : ExprMixin(span), value(value), is_double(is_double) {}

quart::Value FloatExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StringExpr::StringExpr(
    Span span, std::string value
) : ExprMixin(span), value(value) {}

quart::Value StringExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableExpr::VariableExpr(
    Span span, std::string name
) : ExprMixin(span), name(name) {}

quart::Value VariableExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableAssignmentExpr::VariableAssignmentExpr(
    Span span, 
    std::vector<Ident> names, 
    std::unique_ptr<TypeExpr> type, 
    std::unique_ptr<Expr> value, 
    std::string consume_rest,
    bool external,
    bool is_multiple_variables
) : ExprMixin(span), names(names), external(external), 
    is_multiple_variables(is_multiple_variables), consume_rest(consume_rest) {
    this->value = std::move(value);
    this->type = std::move(type);
}

quart::Value VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstExpr::ConstExpr(
    Span span, std::string name, std::unique_ptr<TypeExpr> type, std::unique_ptr<Expr> value
) : ExprMixin(span), name(name) {
    this->value = std::move(value);
    this->type = std::move(type);
}

quart::Value ConstExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ArrayExpr::ArrayExpr(
    Span span, std::vector<std::unique_ptr<Expr>> elements
) : ExprMixin(span) {
    this->elements = std::move(elements);
}

quart::Value ArrayExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UnaryOpExpr::UnaryOpExpr(
    Span span, UnaryOp op, std::unique_ptr<Expr> value
) : ExprMixin(span), op(op) {
    this->value = std::move(value);
}

quart::Value UnaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BinaryOpExpr::BinaryOpExpr(
    Span span, BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right
) : ExprMixin(span), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

quart::Value BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

InplaceBinaryOpExpr::InplaceBinaryOpExpr(
    Span span, BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right
) : ExprMixin(span), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

quart::Value InplaceBinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CallExpr::CallExpr(
    Span span,
    std::unique_ptr<ast::Expr> callee, 
    std::vector<std::unique_ptr<Expr>> args, 
    std::map<std::string, std::unique_ptr<Expr>> kwargs
) : ExprMixin(span) {
    this->callee = std::move(callee);
    this->args = std::move(args);
    this->kwargs = std::move(kwargs);
}

quart::Value CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

std::unique_ptr<Expr>& CallExpr::get(u32 index) {
    return args[index];
}

std::unique_ptr<Expr>& CallExpr::get(const std::string& name) {
    return kwargs[name];
}

ReturnExpr::ReturnExpr(Span span, std::unique_ptr<Expr> value) : ExprMixin(span) {
    this->value = std::move(value);
}

quart::Value ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PrototypeExpr::PrototypeExpr(
    Span span,
    std::string name, 
    std::vector<Argument> args,
    std::unique_ptr<TypeExpr> return_type,
    bool is_c_variadic,
    bool is_operator,
    ExternLinkageSpecifier linkage
) : ExprMixin(span), name(name), is_c_variadic(is_c_variadic), is_operator(is_operator), linkage(linkage) {
    this->args = std::move(args);
    this->return_type = std::move(return_type);
}

quart::Value PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FunctionExpr::FunctionExpr(
    Span span, std::unique_ptr<PrototypeExpr> prototype, std::vector<std::unique_ptr<Expr>> body
) : ExprMixin(span) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

quart::Value FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

DeferExpr::DeferExpr(
    Span span, std::unique_ptr<Expr> expr
) : ExprMixin(span) {
    this->expr = std::move(expr);
}

quart::Value DeferExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IfExpr::IfExpr(
    Span span, std::unique_ptr<Expr> condition, std::unique_ptr<Expr> body, std::unique_ptr<Expr> ebody
) : ExprMixin(span) {
    this->condition = std::move(condition);
    this->body = std::move(body);
    this->ebody = std::move(ebody);
}

quart::Value IfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

WhileExpr::WhileExpr(
    Span span, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body
) : ExprMixin(span) {
    this->condition = std::move(condition);
    this->body = std::move(body);
}

quart::Value WhileExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BreakExpr::BreakExpr(Span span) : ExprMixin(span) {}

quart::Value BreakExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ContinueExpr::ContinueExpr(Span span) : ExprMixin(span) {}

quart::Value ContinueExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StructExpr::StructExpr(
    Span span,
    std::string name,
    bool opaque, 
    std::vector<StructField> fields, 
    std::vector<std::unique_ptr<Expr>> methods
) : ExprMixin(span), name(name), opaque(opaque) {
    this->fields = std::move(fields);
    this->methods = std::move(methods);
}

quart::Value StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstructorExpr::ConstructorExpr(
    Span span, std::unique_ptr<Expr> parent, std::vector<ConstructorField> fields
) : ExprMixin(span) {
    this->parent = std::move(parent);
    this->fields = std::move(fields);
}

quart::Value ConstructorExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

EmptyConstructorExpr::EmptyConstructorExpr(
    Span span, std::unique_ptr<Expr> parent
) : ExprMixin(span) {
    this->parent = std::move(parent);
}

quart::Value EmptyConstructorExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

AttributeExpr::AttributeExpr(
    Span span, std::string attribute, std::unique_ptr<Expr> parent
) : ExprMixin(span), attribute(attribute) {
    this->parent = std::move(parent);
}

quart::Value AttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IndexExpr::IndexExpr(
    Span span, std::unique_ptr<Expr> value, std::unique_ptr<Expr> index
) : ExprMixin(span) {
    this->value = std::move(value);
    this->index = std::move(index);
}

quart::Value IndexExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CastExpr::CastExpr(
    Span span, std::unique_ptr<Expr> value, std::unique_ptr<TypeExpr> to
) : ExprMixin(span) {
    this->value = std::move(value);
    this->to = std::move(to);
}

quart::Value CastExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

SizeofExpr::SizeofExpr(
    Span span, std::unique_ptr<Expr> value
) : ExprMixin(span) {
    this->value = std::move(value);
}

quart::Value SizeofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

OffsetofExpr::OffsetofExpr(
    Span span, std::unique_ptr<Expr> value, std::string field
) : ExprMixin(span), field(field) {
    this->value = std::move(value);
}

quart::Value OffsetofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PathExpr::PathExpr(
    Span span, std::string name, std::unique_ptr<Expr> parent
) : ExprMixin(span), name(name) {
    this->parent = std::move(parent);
}

quart::Value PathExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UsingExpr::UsingExpr(
    Span span, std::vector<std::string> members, std::unique_ptr<Expr> parent
) : ExprMixin(span), members(members) {
    this->parent = std::move(parent);
}

quart::Value UsingExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

TupleExpr::TupleExpr(
    Span span, std::vector<std::unique_ptr<Expr>> elements
) : ExprMixin(span) {
    this->elements = std::move(elements);
}

quart::Value TupleExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

EnumExpr::EnumExpr(
    Span span, std::string name, std::unique_ptr<TypeExpr> type, std::vector<EnumField> fields
) : ExprMixin(span), name(name) {
    this->fields = std::move(fields);
    this->type = std::move(type);
}

quart::Value EnumExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ImportExpr::ImportExpr(
    Span span, std::string name, bool is_wildcard, bool is_relative
) : ExprMixin(span), name(name), is_wildcard(is_wildcard), is_relative(is_relative) {}

quart::Value ImportExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ModuleExpr::ModuleExpr(
    Span span, std::string name, std::vector<std::unique_ptr<Expr>> body
) : ExprMixin(span), name(name) {
    this->body = std::move(body);
}

quart::Value ModuleExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TernaryExpr::TernaryExpr(
    Span span, std::unique_ptr<Expr> condition, std::unique_ptr<Expr> true_expr, std::unique_ptr<Expr> false_expr
) : ExprMixin(span) {
    this->condition = std::move(condition);
    this->true_expr = std::move(true_expr);
    this->false_expr = std::move(false_expr);
}

quart::Value TernaryExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

BuiltinTypeExpr::BuiltinTypeExpr(
    Span span, BuiltinType value
) : TypeExpr(span, TypeKind::Builtin), value(value) {}

quart::Type* BuiltinTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

IntegerTypeExpr::IntegerTypeExpr(
    Span span, std::unique_ptr<Expr> size
) : TypeExpr(span, TypeKind::Integer) {
    this->size = std::move(size);
}

quart::Type* IntegerTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

NamedTypeExpr::NamedTypeExpr(
    Span span, std::string name, std::deque<std::string> parents
) : TypeExpr(span, TypeKind::Named), name(name), parents(parents) {}

quart::Type* NamedTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TupleTypeExpr::TupleTypeExpr(
    Span span, std::vector<std::unique_ptr<TypeExpr>> types
) : TypeExpr(span, TypeKind::Tuple) {
    this->types = std::move(types);
}

quart::Type* TupleTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ArrayTypeExpr::ArrayTypeExpr(
    Span span, std::unique_ptr<TypeExpr> type, std::unique_ptr<Expr> size
) : TypeExpr(span, TypeKind::Array) {
    this->type = std::move(type);
    this->size = std::move(size);
}

quart::Type* ArrayTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

PointerTypeExpr::PointerTypeExpr(
    Span span, std::unique_ptr<TypeExpr> type, bool is_mutable
) : TypeExpr(span, TypeKind::Pointer), is_mutable(is_mutable) {
    this->type = std::move(type);
}

quart::Type* PointerTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

FunctionTypeExpr::FunctionTypeExpr(
    Span span, std::vector<std::unique_ptr<TypeExpr>> args, std::unique_ptr<TypeExpr> ret
) : TypeExpr(span, TypeKind::Function) {
    this->args = std::move(args);
    this->ret = std::move(ret);
}

quart::Type* FunctionTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReferenceTypeExpr::ReferenceTypeExpr(
    Span span, std::unique_ptr<TypeExpr> type, bool is_mutable
) : TypeExpr(span, TypeKind::Reference), is_mutable(is_mutable) {
    this->type = std::move(type);
}

quart::Type* ReferenceTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

GenericTypeExpr::GenericTypeExpr(
    Span span, std::unique_ptr<NamedTypeExpr> parent, std::vector<std::unique_ptr<TypeExpr>> args
) : TypeExpr(span, TypeKind::Generic) {
    this->parent = std::move(parent);
    this->args = std::move(args);
}

quart::Type* GenericTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

TypeAliasExpr::TypeAliasExpr(
    Span span, std::string name, std::unique_ptr<TypeExpr> type, std::vector<GenericParameter> params
) : ExprMixin(span), name(name) {
    this->type = std::move(type);
    this->parameters = std::move(params);
}

quart::Value TypeAliasExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ForExpr::ForExpr(
    Span span, Ident name, std::unique_ptr<Expr> iterable, std::unique_ptr<Expr> body
) : ExprMixin(span), name(name) {
    this->iterable = std::move(iterable);
    this->body = std::move(body);
}   

quart::Value ForExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

RangeForExpr::RangeForExpr(
    Span span, Ident name, std::unique_ptr<Expr> start, std::unique_ptr<Expr> end, std::unique_ptr<Expr> body
) : ExprMixin(span), name(name) {
    this->start = std::move(start);
    this->end = std::move(end);
    this->body = std::move(body);
}

quart::Value RangeForExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ArrayFillExpr::ArrayFillExpr(
    Span span, std::unique_ptr<Expr> element, std::unique_ptr<Expr> count
) : ExprMixin(span) {
    this->element = std::move(element);
    this->count = std::move(count);
}

quart::Value ArrayFillExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

StaticAssertExpr::StaticAssertExpr(
    Span span, std::unique_ptr<Expr> condition, std::string message
) : ExprMixin(span), message(message) {
    this->condition = std::move(condition);
}

quart::Value StaticAssertExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

MaybeExpr::MaybeExpr(
    Span span, std::unique_ptr<Expr> value
) : ExprMixin(span) {
    this->value = std::move(value);
}

quart::Value MaybeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ImplExpr::ImplExpr(
    Span span, std::unique_ptr<TypeExpr> type, ExprList<FunctionExpr> body
) : ExprMixin(span) {
    this->type = std::move(type);
    this->body = std::move(body);
}

quart::Value ImplExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

MatchExpr::MatchExpr(
    Span span, std::unique_ptr<Expr> value, std::vector<MatchArm> arms
) : ExprMixin(span) {
    this->value = std::move(value);
    this->arms = std::move(arms);
}

quart::Value MatchExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}
