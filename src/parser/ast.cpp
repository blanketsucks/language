#include "parser/ast.h"
#include "visitor.h"

#include <vector>

using namespace ast;

BlockExpr::BlockExpr(
    Location start, Location end, std::vector<utils::Ref<Expr>> block
) : ExprMixin(start, end) {
    this->block = std::move(block);
}

Value BlockExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IntegerExpr::IntegerExpr(
    Location start, Location end, std::string value, int bits, bool is_float
) : ExprMixin(start, end), value(value), bits(bits), is_float(is_float) {}

Value IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CharExpr::CharExpr(Location start, Location end, char value) : ExprMixin(start, end), value(value) {}

Value CharExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FloatExpr::FloatExpr(
    Location start, Location end, double value, bool is_double
) : ExprMixin(start, end), value(value), is_double(is_double) {}

Value FloatExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StringExpr::StringExpr(
    Location start, Location end, std::string value
) : ExprMixin(start, end), value(value) {}

Value StringExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableExpr::VariableExpr(
    Location start, Location end, std::string name
) : ExprMixin(start, end), name(name) {}

Value VariableExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableAssignmentExpr::VariableAssignmentExpr(
    Location start, 
    Location end, 
    std::vector<std::string> names, 
    utils::Ref<TypeExpr> type, 
    utils::Ref<Expr> value, 
    std::string consume_rest,
    bool external,
    bool is_multiple_variables,
    bool is_immutable
) : ExprMixin(start, end), names(names), external(external), 
    is_multiple_variables(is_multiple_variables), is_immutable(is_immutable), consume_rest(consume_rest) {
    this->value = std::move(value);
    this->type = std::move(type);
}

Value VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstExpr::ConstExpr(
    Location start, Location end, std::string name, utils::Ref<TypeExpr> type, utils::Ref<Expr> value
) : ExprMixin(start, end), name(name) {
    this->value = std::move(value);
    this->type = std::move(type);
}

Value ConstExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ArrayExpr::ArrayExpr(
    Location start, Location end, std::vector<utils::Ref<Expr>> elements
) : ExprMixin(start, end) {
    this->elements = std::move(elements);
}

Value ArrayExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UnaryOpExpr::UnaryOpExpr(
    Location start, Location end, TokenKind op, utils::Ref<Expr> value
) : ExprMixin(start, end), op(op) {
    this->value = std::move(value);
}

Value UnaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BinaryOpExpr::BinaryOpExpr(
    Location start, Location end, TokenKind op, utils::Ref<Expr> left, utils::Ref<Expr> right
) : ExprMixin(start, end), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

Value BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

InplaceBinaryOpExpr::InplaceBinaryOpExpr(
    Location start, Location end, TokenKind op, utils::Ref<Expr> left, utils::Ref<Expr> right
) : ExprMixin(start, end), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

Value InplaceBinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CallExpr::CallExpr(
    Location start, 
    Location end, 
    utils::Ref<ast::Expr> callee, 
    std::vector<utils::Ref<Expr>> args, 
    std::map<std::string, utils::Ref<Expr>> kwargs
) : ExprMixin(start, end) {
    this->callee = std::move(callee);
    this->args = std::move(args);
    this->kwargs = std::move(kwargs);
}

Value CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReturnExpr::ReturnExpr(Location start, Location end, utils::Ref<Expr> value) : ExprMixin(start, end) {
    this->value = std::move(value);
}

Value ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PrototypeExpr::PrototypeExpr(
    Location start, 
    Location end, 
    std::string name, 
    std::vector<Argument> args,
    utils::Ref<TypeExpr> return_type,
    bool is_variadic,
    bool is_operator,
    ExternLinkageSpecifier linkage
) : ExprMixin(start, end), name(name), is_variadic(is_variadic), is_operator(is_operator), linkage(linkage) {
    this->args = std::move(args);
    this->return_type = std::move(return_type);
}

Value PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FunctionExpr::FunctionExpr(
    Location start, Location end, utils::Ref<PrototypeExpr> prototype, utils::Ref<Expr> body
) : ExprMixin(start, end) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

Value FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

DeferExpr::DeferExpr(
    Location start, Location end, utils::Ref<Expr> expr
) : ExprMixin(start, end) {
    this->expr = std::move(expr);
}

Value DeferExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IfExpr::IfExpr(
    Location start, Location end, utils::Ref<Expr> condition, utils::Ref<Expr> body, utils::Ref<Expr> ebody
) : ExprMixin(start, end) {
    this->condition = std::move(condition);
    this->body = std::move(body);
    this->ebody = std::move(ebody);
}

Value IfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

WhileExpr::WhileExpr(
    Location start, Location end, utils::Ref<Expr> condition, utils::Ref<BlockExpr> body
) : ExprMixin(start, end) {
    this->condition = std::move(condition);
    this->body = std::move(body);
}

Value WhileExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ForExpr::ForExpr(
    Location start,
    Location end, 
    utils::Ref<Expr> start_,
    utils::Ref<Expr> end_,
    utils::Ref<Expr> step, 
    utils::Ref<Expr> body
) : ExprMixin(start, end) {
    this->start = std::move(start_);
    this->end = std::move(end_);
    this->step = std::move(step);
    this->body = std::move(body);
}

Value ForExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BreakExpr::BreakExpr(Location start, Location end) : ExprMixin(start, end) {}

Value BreakExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ContinueExpr::ContinueExpr(Location start, Location end) : ExprMixin(start, end) {}

Value ContinueExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StructExpr::StructExpr(
    Location start, 
    Location end, 
    std::string name,
    bool opaque, 
    std::vector<utils::Ref<Expr>> parents,
    std::map<std::string, StructField> fields, 
    std::vector<utils::Ref<Expr>> methods
) : ExprMixin(start, end), name(name), opaque(opaque) {
    this->fields = std::move(fields);
    this->methods = std::move(methods);
    this->parents = std::move(parents);
}

Value StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstructorExpr::ConstructorExpr(
    Location start, Location end, utils::Ref<Expr> parent, std::vector<ConstructorField> fields
) : ExprMixin(start, end) {
    this->parent = std::move(parent);
    this->fields = std::move(fields);
}

Value ConstructorExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

AttributeExpr::AttributeExpr(
    Location start, Location end, std::string attribute, utils::Ref<Expr> parent
) : ExprMixin(start, end), attribute(attribute) {
    this->parent = std::move(parent);
}

Value AttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ElementExpr::ElementExpr(
    Location start, Location end, utils::Ref<Expr> value, utils::Ref<Expr> index
) : ExprMixin(start, end) {
    this->value = std::move(value);
    this->index = std::move(index);
}

Value ElementExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CastExpr::CastExpr(
    Location start, Location end, utils::Ref<Expr> value, utils::Ref<TypeExpr> to
) : ExprMixin(start, end) {
    this->value = std::move(value);
    this->to = std::move(to);
}

Value CastExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

SizeofExpr::SizeofExpr(
    Location start, Location end, utils::Ref<Expr> value
) : ExprMixin(start, end) {
    this->value = std::move(value);
}

Value SizeofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

OffsetofExpr::OffsetofExpr(
    Location start, Location end, utils::Ref<Expr> value, std::string field
) : ExprMixin(start, end), field(field) {
    this->value = std::move(value);
}

Value OffsetofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceExpr::NamespaceExpr(
    Location start, Location end, std::string name, std::deque<std::string> parents, std::vector<utils::Ref<Expr>> members
) : ExprMixin(start, end), name(name), parents(parents) {
    this->members = std::move(members);
}

Value NamespaceExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceAttributeExpr::NamespaceAttributeExpr(
    Location start, Location end, std::string attribute, utils::Ref<Expr> parent
) : ExprMixin(start, end), attribute(attribute) {
    this->parent = std::move(parent);
}

Value NamespaceAttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UsingExpr::UsingExpr(
    Location start, Location end, std::vector<std::string> members, utils::Ref<Expr> parent
) : ExprMixin(start, end), members(members) {
    this->parent = std::move(parent);
}

Value UsingExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

TupleExpr::TupleExpr(
    Location start, Location end, std::vector<utils::Ref<Expr>> elements
) : ExprMixin(start, end) {
    this->elements = std::move(elements);
}

Value TupleExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

EnumExpr::EnumExpr(
    Location start, Location end, std::string name, utils::Ref<TypeExpr> type, std::vector<EnumField> fields
) : ExprMixin(start, end), name(name) {
    this->fields = std::move(fields);
    this->type = std::move(type);
}

Value EnumExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

WhereExpr::WhereExpr(
    Location start, Location end, utils::Ref<Expr> expr
) : ExprMixin(start, end) {
    this->expr = std::move(expr);
}

Value WhereExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ImportExpr::ImportExpr(
    Location start, Location end, std::string name, std::deque<std::string> parents, bool is_wildcard, bool is_relative
) : ExprMixin(start, end), name(name), parents(parents), is_wildcard(is_wildcard), is_relative(is_relative) {}

Value ImportExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TernaryExpr::TernaryExpr(
    Location start, Location end, utils::Ref<Expr> condition, utils::Ref<Expr> true_expr, utils::Ref<Expr> false_expr
) : ExprMixin(start, end) {
    this->condition = std::move(condition);
    this->true_expr = std::move(true_expr);
    this->false_expr = std::move(false_expr);
}

Value TernaryExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

BuiltinTypeExpr::BuiltinTypeExpr(
    Location start, Location end, BuiltinType value
) : TypeExpr(start, end, TypeKind::Builtin), value(value) {}

Value BuiltinTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

IntegerTypeExpr::IntegerTypeExpr(
    Location start, Location end, utils::Ref<Expr> size
) : TypeExpr(start, end, TypeKind::Integer) {
    this->size = std::move(size);
}

Value IntegerTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

NamedTypeExpr::NamedTypeExpr(
    Location start, Location end, std::string name, std::deque<std::string> parents
) : TypeExpr(start, end, TypeKind::Named), name(name), parents(parents) {}

Value NamedTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TupleTypeExpr::TupleTypeExpr(
    Location start, Location end, std::vector<utils::Ref<TypeExpr>> elements
) : TypeExpr(start, end, TypeKind::Tuple) {
    this->elements = std::move(elements);
}

Value TupleTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ArrayTypeExpr::ArrayTypeExpr(
    Location start, Location end, utils::Ref<TypeExpr> element, utils::Ref<Expr> size
) : TypeExpr(start, end, TypeKind::Array) {
    this->element = std::move(element);
    this->size = std::move(size);
}

Value ArrayTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

PointerTypeExpr::PointerTypeExpr(
    Location start, Location end, utils::Ref<TypeExpr> element
) : TypeExpr(start, end, TypeKind::Pointer) {
    this->element = std::move(element);
}

Value PointerTypeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

FunctionTypeExpr::FunctionTypeExpr(
    Location start, Location end, std::vector<utils::Ref<TypeExpr>> args, utils::Ref<TypeExpr> ret
) : TypeExpr(start, end, TypeKind::Function) {
    this->args = std::move(args);
    this->ret = std::move(ret);
}

Value FunctionTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReferenceTypeExpr::ReferenceTypeExpr(
    Location start, Location end, utils::Ref<TypeExpr> type
) : TypeExpr(start, end, TypeKind::Reference) {
    this->type = std::move(type);
}

Value ReferenceTypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ForeachExpr::ForeachExpr(
    Location start, Location end, std::string name, utils::Ref<Expr> iterable, utils::Ref<Expr> body
) : ExprMixin(start, end), name(name) {
    this->iterable = std::move(iterable);
    this->body = std::move(body);
}   

Value ForeachExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

ArrayFillExpr::ArrayFillExpr(
    Location start, Location end, utils::Ref<Expr> element, utils::Ref<Expr> count
) : ExprMixin(start, end) {
    this->element = std::move(element);
    this->count = std::move(count);
}

Value ArrayFillExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

TypeAliasExpr::TypeAliasExpr(
    Location start, Location end, std::string name, utils::Ref<TypeExpr> type
) : ExprMixin(start, end), name(name) {
    this->type = std::move(type);
}

Value TypeAliasExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

StaticAssertExpr::StaticAssertExpr(
    Location start, Location end, utils::Ref<Expr> condition, std::string message
) : ExprMixin(start, end), message(message) {
    this->condition = std::move(condition);
}

Value StaticAssertExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}

MaybeExpr::MaybeExpr(
    Location start, Location end, utils::Ref<Expr> value
) : ExprMixin(start, end) {
    this->value = std::move(value);
}

Value MaybeExpr::accept(Visitor &visitor) {
    return visitor.visit(this);
}