#include <quart/parser/ast.h>
#include <quart/visitor.h>

#include <vector>

#define EXPR_LIST(Op)                       \
    Op(BlockExpr)                           \
    Op(ExternBlockExpr)                     \
    Op(IntegerExpr)                         \
    Op(CharExpr)                            \
    Op(FloatExpr)                           \
    Op(StringExpr)                          \
    Op(VariableExpr)                        \
    Op(VariableAssignmentExpr)              \
    Op(ConstExpr)                           \
    Op(ArrayExpr)                           \
    Op(UnaryOpExpr)                         \
    Op(BinaryOpExpr)                        \
    Op(InplaceBinaryOpExpr)                 \
    Op(CallExpr)                            \
    Op(ReturnExpr)                          \
    Op(PrototypeExpr)                       \
    Op(FunctionExpr)                        \
    Op(DeferExpr)                           \
    Op(IfExpr)                              \
    Op(WhileExpr)                           \
    Op(BreakExpr)                           \
    Op(ContinueExpr)                        \
    Op(StructExpr)                          \
    Op(ConstructorExpr)                     \
    Op(EmptyConstructorExpr)                \
    Op(AttributeExpr)                       \
    Op(IndexExpr)                           \
    Op(CastExpr)                            \
    Op(SizeofExpr)                          \
    Op(OffsetofExpr)                        \
    Op(PathExpr)                            \
    Op(UsingExpr)                           \
    Op(TupleExpr)                           \
    Op(EnumExpr)                            \
    Op(ImportExpr)                          \
    Op(ModuleExpr)                          \
    Op(TernaryExpr)                         \
    Op(ForExpr)                             \
    Op(RangeForExpr)                        \
    Op(ArrayFillExpr)                       \
    Op(StaticAssertExpr)                    \
    Op(MaybeExpr)                           \
    Op(ImplExpr)                            \
    Op(TypeAliasExpr)                       \
    Op(MatchExpr)                           \
    Op(ReferenceExpr)

#define TYPE_EXPR_LIST(Op)                  \
    Op(BuiltinTypeExpr)                     \
    Op(IntegerTypeExpr)                     \
    Op(NamedTypeExpr)                       \
    Op(TupleTypeExpr)                       \
    Op(ArrayTypeExpr)                       \
    Op(PointerTypeExpr)                     \
    Op(FunctionTypeExpr)                    \
    Op(ReferenceTypeExpr)                   \
    Op(GenericTypeExpr)                     

#define EXPR_VISIT(name) quart::Value name::accept(Visitor& visitor) { return visitor.visit(this); }
#define TYPE_EXPR_VISIT(name) quart::Type* name::accept(Visitor& visitor) { return visitor.visit(this); }

using namespace quart::ast;

EXPR_LIST(EXPR_VISIT)
TYPE_EXPR_LIST(TYPE_EXPR_VISIT)