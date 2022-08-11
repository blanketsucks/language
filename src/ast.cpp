#include "ast.h"

#include "objects.h"
#include "visitor.h"

using namespace ast;

ExprKind::ExprKind(Value value) : value(value) {}

bool ExprKind::operator==(Value other) {
    return this->value == other;
}

bool ExprKind::operator==(ExprKind other) {
    return this->value == other.value;
}

bool ExprKind::operator!=(Value other) {
    return this->value != other;
}

bool ExprKind::in(ExprKind other, ...)  {
    va_list args;
    va_start(args, other);

    ExprKind* arg = va_arg(args, ExprKind*);
    while (arg != nullptr) {
        if (this->value == arg->value) {
            va_end(args);
            return true;
        }
        arg = va_arg(args, ExprKind*);
    }

    va_end(args);
    return false;
}

bool ExprKind::in(std::vector<ExprKind> others) {
    for (auto kind : others) {
        if (kind == this->value) {
            return true;
        }
    }

    return false;
}

BlockExpr::BlockExpr(
    Location start, Location end, std::vector<std::unique_ptr<Expr>> block
) : Expr(start, end, ExprKind::Block) {
    this->block = std::move(block);
}

Value BlockExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IntegerExpr::IntegerExpr(
    Location start, Location end, int value, int bits
) : Expr(start, end, ExprKind::Integer), value(value), bits(bits) {}

Value IntegerExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FloatExpr::FloatExpr(
    Location start, Location end, float value
) : Expr(start, end, ExprKind::Float), value(value) {}

Value FloatExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StringExpr::StringExpr(
    Location start, Location end, std::string value
) : Expr(start, end, ExprKind::String), value(value) {}

Value StringExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableExpr::VariableExpr(
    Location start, Location end, std::string name
) : Expr(start, end, ExprKind::Variable), name(name) {}

Value VariableExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

VariableAssignmentExpr::VariableAssignmentExpr(
    Location start, Location end, std::string name, Type* type, std::unique_ptr<Expr> value, bool external
) : Expr(start, end, ExprKind::VariableAssignment), name(name), type(type), external(external) {
    this->value = std::move(value);
}

Value VariableAssignmentExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstExpr::ConstExpr(
    Location start, Location end, std::string name, Type* type, std::unique_ptr<Expr> value
) : VariableAssignmentExpr(start, end, name, type, std::move(value)) {}

Value ConstExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ArrayExpr::ArrayExpr(
    Location start, Location end, std::vector<std::unique_ptr<Expr>> elements
) : Expr(start, end, ExprKind::Array) {
    this->elements = std::move(elements);
}

Value ArrayExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

UnaryOpExpr::UnaryOpExpr(
    Location start, Location end, TokenType op, std::unique_ptr<Expr> value
) : Expr(start, end, ExprKind::UnaryOp), op(op) {
    this->value = std::move(value);
}

Value UnaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

BinaryOpExpr::BinaryOpExpr(
    Location start, Location end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right
) : Expr(start, end, ExprKind::BinaryOp), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

Value BinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

InplaceBinaryOpExpr::InplaceBinaryOpExpr(
    Location start, Location end, TokenType op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right
) : Expr(start, end, ExprKind::InplaceBinaryOp), op(op) {
    this->left = std::move(left);
    this->right = std::move(right);
}

Value InplaceBinaryOpExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CallExpr::CallExpr(
    Location start, Location end, std::unique_ptr<ast::Expr> callee, std::vector<std::unique_ptr<Expr>> args
) : Expr(start, end, ExprKind::Call) {
    this->callee = std::move(callee);
    this->args = std::move(args);
}

Value CallExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ReturnExpr::ReturnExpr(Location start, Location end, std::unique_ptr<Expr> value) : Expr(start, end, ExprKind::Return) {
    this->value = std::move(value);
}

Value ReturnExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

PrototypeExpr::PrototypeExpr(
    Location start, Location end, std::string name, Type* return_type, std::vector<Argument> args, bool has_varargs
) : Expr(start, end, ExprKind::Prototype), name(name), return_type(return_type), has_varargs(has_varargs) {
    this->args = std::move(args);
}

Value PrototypeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

FunctionExpr::FunctionExpr(
    Location start, Location end, std::unique_ptr<PrototypeExpr> prototype, std::unique_ptr<BlockExpr> body
) : Expr(start, end, ExprKind::Function) {
    this->prototype = std::move(prototype);
    this->body = std::move(body);
}

Value FunctionExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

DeferExpr::DeferExpr(
    Location start, Location end, std::unique_ptr<Expr> expr
) : Expr(start, end, ExprKind::Defer) {
    this->expr = std::move(expr);
}

Value DeferExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

IfExpr::IfExpr(
    Location start, Location end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body, std::unique_ptr<BlockExpr> ebody
) : Expr(start, end, ExprKind::If) {
    this->condition = std::move(condition);
    this->body = std::move(body);
    this->ebody = std::move(ebody);
}

Value IfExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

WhileExpr::WhileExpr(
    Location start, Location end, std::unique_ptr<Expr> condition, std::unique_ptr<BlockExpr> body
) : Expr(start, end, ExprKind::While) {
    this->condition = std::move(condition);
    this->body = std::move(body);
}

Value WhileExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ForExpr::ForExpr(
    Location start, Location end, std::string name, std::unique_ptr<Expr> iterator, std::unique_ptr<BlockExpr> body
) : Expr(start, end, ExprKind::For) {
    this->name = name;
    this->iterator = std::move(iterator);
    this->body = std::move(body);
}

Value ForExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

StructExpr::StructExpr(
    Location start, 
    Location end, 
    std::string name,
    bool packed, 
    bool opaque, 
    std::vector<std::unique_ptr<Expr>> parents,
    std::map<std::string, StructField> fields, 
    std::vector<std::unique_ptr<Expr>> methods
) : Expr(start, end, ExprKind::Struct), name(name), packed(packed), opaque(opaque) {
    this->fields = std::move(fields);
    this->methods = std::move(methods);
    this->parents = std::move(parents);
}

Value StructExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ConstructorExpr::ConstructorExpr(
    Location start, Location end, std::unique_ptr<Expr> parent, std::map<std::string, std::unique_ptr<Expr>> fields
) : Expr(start, end, ExprKind::Constructor) {
    this->parent = std::move(parent);
    this->fields = std::move(fields);
}

Value ConstructorExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

AttributeExpr::AttributeExpr(
    Location start, Location end, std::string attribute, std::unique_ptr<Expr> parent
) : Expr(start, end, ExprKind::Attribute), attribute(attribute) {
    this->parent = std::move(parent);
}

Value AttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

ElementExpr::ElementExpr(
    Location start, Location end, std::unique_ptr<Expr> value, std::unique_ptr<Expr> index
) : Expr(start, end, ExprKind::Element) {
    this->value = std::move(value);
    this->index = std::move(index);
}

Value ElementExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

CastExpr::CastExpr(
    Location start, Location end, std::unique_ptr<Expr> value, Type* to
) : Expr(start, end, ExprKind::Cast), to(to) {
    this->value = std::move(value);
}

Value CastExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

SizeofExpr::SizeofExpr(Location start, Location end, Type* type) : Expr(start, end, ExprKind::Sizeof), type(type) {}

Value SizeofExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

InlineAssemblyExpr::InlineAssemblyExpr(
    Location start, 
    Location end, 
    std::string assembly, 
    InlineAssemblyConstraint inputs, 
    InlineAssemblyConstraint outputs, 
    std::vector<std::string> clobbers
) : Expr(start, end, ExprKind::Assembly), assembly(assembly) {
    this->inputs = std::move(inputs);
    this->outputs = std::move(outputs);
    this->clobbers = clobbers;
}

Value InlineAssemblyExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceExpr::NamespaceExpr(
    Location start, Location end, std::string name, std::vector<std::unique_ptr<Expr>> members
) : Expr(start, end, ExprKind::Namespace), name(name) {
    this->members = std::move(members);
}

Value NamespaceExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

NamespaceAttributeExpr::NamespaceAttributeExpr(
    Location start, Location end, std::string attribute, std::unique_ptr<Expr> parent
) : Expr(start, end, ExprKind::NamespaceAttribute), attribute(attribute) {
    this->parent = std::move(parent);
}

UsingExpr::UsingExpr(
    Location start, Location end, std::vector<std::string> members, std::unique_ptr<Expr> parent
) : Expr(start, end, ExprKind::Using), members(members) {
    this->parent = std::move(parent);
}

Value UsingExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}

Value NamespaceAttributeExpr::accept(Visitor& visitor) {
    return visitor.visit(this);
}
