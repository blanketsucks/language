#include "parser/ast.h"
#include "visitor.h"

#include <vector>

using namespace ast;

BlockExpr::BlockExpr(
    Span span, std::vector<utils::Scope<Expr>> block
) : ExprMixin(span) {
    this->block = std::move(block);
}

Value BlockExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IntegerExpr::IntegerExpr(
    Span span, std::string value, int bits, bool is_float
) : ExprMixin(span), value(value), bits(bits), is_float(is_float) {}

Value IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CharExpr::CharExpr(Span span, char value) : ExprMixin(span), value(value) {}

Value CharExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FloatExpr::FloatExpr(
    Span span, double value, bool is_double
) : ExprMixin(span), value(value), is_double(is_double) {}

Value FloatExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StringExpr::StringExpr(
    Span span, std::string value
) : ExprMixin(span), value(value) {}

Value StringExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableExpr::VariableExpr(
    Span span, std::string name
) : ExprMixin(span), name(name) {}

Value VariableExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableAssignmentExpr::VariableAssignmentExpr(
    Span span, 
    std::vector<std::string> names, 
    utils::Scope<TypeExpr> type, 
    utils::Scope<Expr> value, 
    std::string consume_rest,
    bool external,
    bool is_multiple_variables,
    bool is_immutable
) : ExprMixin(span), names(names), external(external), 
    is_multiple_variables(is_multiple_variables), is_immutable(is_immutable), consume_rest(consume_rest) {
    this->value = std::move(value);
    this->type = std::move(type);
}

Value VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstExpr::ConstExpr(
    Span span, std::string name, utils::Scope<TypeExpr> type, utils::Scope<Expr> value
) : ExprMixin(span), name(name) {
    this->value = std::move(value);
    this->type = std::move(type);
}

Value ConstExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ArrayExpr::ArrayExpr(
    Span span, std::vector<utils::Scope<Expr>> elements
) : ExprMixin(span) {
    this->elements = std::move(elements);
}

Value ArrayExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UnaryOpExpr::UnaryOpExpr(
    Span span, TokenKind op, utils::Scope<Expr> value
) : ExprMixin(span), op(op) {
    this->value = std::move(value);
}

Value UnaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BinaryOpExpr::BinaryOpExpr(
    Span span, TokenKind op, utils::Scope<Expr> left, utils::Scope<Expr> right
) : ExprMixin(span), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

Value BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

InplaceBinaryOpExpr::InplaceBinaryOpExpr(
    Span span, TokenKind op, utils::Scope<Expr> left, utils::Scope<Expr> right
) : ExprMixin(span), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

Value InplaceBinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CallExpr::CallExpr(
    Span span,
    utils::Scope<ast::Expr> callee, 
    std::vector<utils::Scope<Expr>> args, 
    std::map<std::string, utils::Scope<Expr>> kwargs
) : ExprMixin(span) {
    this->callee = std::move(callee);
    this->args = std::move(args);
    this->kwargs = std::move(kwargs);
}

Value CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

utils::Scope<ast::Expr>& CallExpr::get(uint32_t index) {
    return args[index];
}

utils::Scope<ast::Expr>& CallExpr::get(const std::string& name) {
    return kwargs[name];
}

ReturnExpr::ReturnExpr(Span span, utils::Scope<Expr> value) : ExprMixin(span) {
    this->value = std::move(value);
}

Value ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PrototypeExpr::PrototypeExpr(
    Span span,
    std::string name, 
    std::vector<Argument> args,
    utils::Scope<TypeExpr> return_type,
    bool is_c_variadic,
    bool is_operator,
    ExternLinkageSpecifier linkage
) : ExprMixin(span), name(name), is_c_variadic(is_c_variadic), is_operator(is_operator), linkage(linkage) {
    this->args = std::move(args);
    this->return_type = std::move(return_type);
}

Value PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FunctionExpr::FunctionExpr(
    Span span, utils::Scope<PrototypeExpr> prototype, std::vector<utils::Scope<Expr>> body
) : ExprMixin(span) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

Value FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

DeferExpr::DeferExpr(
    Span span, utils::Scope<Expr> expr
) : ExprMixin(span) {
    this->expr = std::move(expr);
}

Value DeferExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IfExpr::IfExpr(
    Span span, utils::Scope<Expr> condition, utils::Scope<Expr> body, utils::Scope<Expr> ebody
) : ExprMixin(span) {
    this->condition = std::move(condition);
    this->body = std::move(body);
    this->ebody = std::move(ebody);
}

Value IfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

WhileExpr::WhileExpr(
    Span span, utils::Scope<Expr> condition, utils::Scope<BlockExpr> body
) : ExprMixin(span) {
    this->condition = std::move(condition);
    this->body = std::move(body);
}

Value WhileExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ForExpr::ForExpr(
    Span span,
    utils::Scope<Expr> start_,
    utils::Scope<Expr> end_,
    utils::Scope<Expr> step, 
    utils::Scope<Expr> body
) : ExprMixin(span) {
    this->start = std::move(start_);
    this->end = std::move(end_);
    this->step = std::move(step);
    this->body = std::move(body);
}

Value ForExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BreakExpr::BreakExpr(Span span) : ExprMixin(span) {}

Value BreakExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ContinueExpr::ContinueExpr(Span span) : ExprMixin(span) {}

Value ContinueExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StructExpr::StructExpr(
    Span span,
    std::string name,
    bool opaque, 
    std::vector<utils::Scope<Expr>> parents,
    std::vector<StructField> fields, 
    std::vector<utils::Scope<Expr>> methods
) : ExprMixin(span), name(name), opaque(opaque) {
    this->fields = std::move(fields);
    this->methods = std::move(methods);
    this->parents = std::move(parents);
}

Value StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstructorExpr::ConstructorExpr(
    Span span, utils::Scope<Expr> parent, std::vector<ConstructorField> fields
) : ExprMixin(span) {
    this->parent = std::move(parent);
    this->fields = std::move(fields);
}

Value ConstructorExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

AttributeExpr::AttributeExpr(
    Span span, std::string attribute, utils::Scope<Expr> parent
) : ExprMixin(span), attribute(attribute) {
    this->parent = std::move(parent);
}

Value AttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ElementExpr::ElementExpr(
    Span span, utils::Scope<Expr> value, utils::Scope<Expr> index
) : ExprMixin(span) {
    this->value = std::move(value);
    this->index = std::move(index);
}

Value ElementExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CastExpr::CastExpr(
    Span span, utils::Scope<Expr> value, utils::Scope<TypeExpr> to
) : ExprMixin(span) {
    this->value = std::move(value);
    this->to = std::move(to);
}

Value CastExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

SizeofExpr::SizeofExpr(
    Span span, utils::Scope<Expr> value
) : ExprMixin(span) {
    this->value = std::move(value);
}

Value SizeofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

OffsetofExpr::OffsetofExpr(
    Span span, utils::Scope<Expr> value, std::string field
) : ExprMixin(span), field(field) {
    this->value = std::move(value);
}

Value OffsetofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceExpr::NamespaceExpr(
    Span span, std::string name, std::deque<std::string> parents, std::vector<utils::Scope<Expr>> members
) : ExprMixin(span), name(name), parents(parents) {
    this->members = std::move(members);
}

Value NamespaceExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceAttributeExpr::NamespaceAttributeExpr(
    Span span, std::string attribute, utils::Scope<Expr> parent
) : ExprMixin(span), attribute(attribute) {
    this->parent = std::move(parent);
}

Value NamespaceAttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UsingExpr::UsingExpr(
    Span span, std::vector<std::string> members, utils::Scope<Expr> parent
) : ExprMixin(span), members(members) {
    this->parent = std::move(parent);
}

Value UsingExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

TupleExpr::TupleExpr(
    Span span, std::vector<utils::Scope<Expr>> elements
) : ExprMixin(span) {
    this->elements = std::move(elements);
}

Value TupleExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

EnumExpr::EnumExpr(
    Span span, std::string name, utils::Scope<TypeExpr> type, std::vector<EnumField> fields
) : ExprMixin(span), name(name) {
    this->fields = std::move(fields);
    this->type = std::move(type);
}

Value EnumExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ImportExpr::ImportExpr(
    Span span, std::string name, bool is_wildcard, bool is_relative
) : ExprMixin(span), name(name), is_wildcard(is_wildcard), is_relative(is_relative) {}

Value ImportExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TernaryExpr::TernaryExpr(
    Span span, utils::Scope<Expr> condition, utils::Scope<Expr> true_expr, utils::Scope<Expr> false_expr
) : ExprMixin(span) {
    this->condition = std::move(condition);
    this->true_expr = std::move(true_expr);
    this->false_expr = std::move(false_expr);
}

Value TernaryExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

BuiltinTypeExpr::BuiltinTypeExpr(
    Span span, BuiltinType value
) : TypeExpr(span, TypeKind::Builtin), value(value) {}

Value BuiltinTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

IntegerTypeExpr::IntegerTypeExpr(
    Span span, utils::Scope<Expr> size
) : TypeExpr(span, TypeKind::Integer) {
    this->size = std::move(size);
}

Value IntegerTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

NamedTypeExpr::NamedTypeExpr(
    Span span, std::string name, std::deque<std::string> parents
) : TypeExpr(span, TypeKind::Named), name(name), parents(parents) {}

Value NamedTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TupleTypeExpr::TupleTypeExpr(
    Span span, std::vector<utils::Scope<TypeExpr>> elements
) : TypeExpr(span, TypeKind::Tuple) {
    this->elements = std::move(elements);
}

Value TupleTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ArrayTypeExpr::ArrayTypeExpr(
    Span span, utils::Scope<TypeExpr> element, utils::Scope<Expr> size
) : TypeExpr(span, TypeKind::Array) {
    this->element = std::move(element);
    this->size = std::move(size);
}

Value ArrayTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

PointerTypeExpr::PointerTypeExpr(
    Span span, utils::Scope<TypeExpr> element
) : TypeExpr(span, TypeKind::Pointer) {
    this->element = std::move(element);
}

Value PointerTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

FunctionTypeExpr::FunctionTypeExpr(
    Span span, std::vector<utils::Scope<TypeExpr>> args, utils::Scope<TypeExpr> ret
) : TypeExpr(span, TypeKind::Function) {
    this->args = std::move(args);
    this->ret = std::move(ret);
}

Value FunctionTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReferenceTypeExpr::ReferenceTypeExpr(
    Span span, utils::Scope<TypeExpr> type
) : TypeExpr(span, TypeKind::Reference) {
    this->type = std::move(type);
}

Value ReferenceTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ForeachExpr::ForeachExpr(
    Span span, std::string name, utils::Scope<Expr> iterable, utils::Scope<Expr> body
) : ExprMixin(span), name(name) {
    this->iterable = std::move(iterable);
    this->body = std::move(body);
}   

Value ForeachExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ArrayFillExpr::ArrayFillExpr(
    Span span, utils::Scope<Expr> element, utils::Scope<Expr> count
) : ExprMixin(span) {
    this->element = std::move(element);
    this->count = std::move(count);
}

Value ArrayFillExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TypeAliasExpr::TypeAliasExpr(
    Span span, std::string name, utils::Scope<TypeExpr> type
) : ExprMixin(span), name(name) {
    this->type = std::move(type);
}

Value TypeAliasExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

StaticAssertExpr::StaticAssertExpr(
    Span span, utils::Scope<Expr> condition, std::string message
) : ExprMixin(span), message(message) {
    this->condition = std::move(condition);
}

Value StaticAssertExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

MaybeExpr::MaybeExpr(
    Span span, utils::Scope<Expr> value
) : ExprMixin(span) {
    this->value = std::move(value);
}

Value MaybeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}