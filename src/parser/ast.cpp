#include "parser/ast.h"

#include "objects.h"
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
    Location start, Location end, int value, int bits
) : ExprMixin(start, end), value(value), bits(bits) {}

Value IntegerExpr::accept(Visitor& visitor) {
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
    Type* type, 
    utils::Ref<Expr> value, 
    bool external,
    bool is_multiple_variables
) : ExprMixin(start, end), names(names), type(type), external(external), is_multiple_variables(is_multiple_variables) {
    this->value = std::move(value);
}

Value VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstExpr::ConstExpr(
    Location start, Location end, std::string name, Type* type, utils::Ref<Expr> value
) : ExprMixin(start, end), type(type), name(name) {
    this->value = std::move(value);
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
    bool is_variadic,
    Type* return_type, 
    ExternLinkageSpecifier linkage
) : ExprMixin(start, end), name(name), is_variadic(is_variadic), return_type(return_type), linkage(linkage) {
    this->args = std::move(args);
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
    std::vector<utils::Ref<Expr>> methods,
    StructType* type
) : ExprMixin(start, end), name(name), opaque(opaque), type(type) {
    this->fields = std::move(fields);
    this->methods = std::move(methods);
    this->parents = std::move(parents);
}

Value StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstructorExpr::ConstructorExpr(
    Location start, Location end, utils::Ref<Expr> parent, std::map<std::string, utils::Ref<Expr>> fields
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
    Location start, Location end, utils::Ref<Expr> value, Type* to
) : ExprMixin(start, end), to(to) {
    this->value = std::move(value);
}

Value CastExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

SizeofExpr::SizeofExpr(
    Location start, Location end, Type* type, utils::Ref<Expr> value
) : ExprMixin(start, end), type(type) {
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
    Location start, Location end, std::string name, Type* type, std::vector<EnumField> fields
) : ExprMixin(start, end), name(name), type(type) {
    this->fields = std::move(fields);
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
    Location start, Location end, std::string name, std::deque<std::string> parents, bool is_wildcard
) : ExprMixin(start, end), name(name), parents(parents), is_wildcard(is_wildcard) {}

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