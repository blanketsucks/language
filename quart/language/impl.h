#pragma once

#include <quart/common.h>
#include <quart/language/scopes.h>

namespace quart {

struct ImplCondition;

// union ImplConditionData {
//     // for Pointer
//     struct {
//         size_t depth;
//     };

//     // for FunctionPramater
//     struct {
//         size_t index;
//         OwnPtr<ImplCondition> inner;
//     };
// };

struct ImplCondition {
    struct Result {
        bool satisfied;
        quart::Type* type;

        String name;  
    };

    enum Type {
        Pointer,
        Reference,
        FunctionParameter,
        FunctionReturn
    };

    static OwnPtr<ImplCondition> create(String name, Type type, size_t index = 0, OwnPtr<ImplCondition> inner = nullptr) {
        return OwnPtr<ImplCondition>(new ImplCondition(move(name), type, index, move(inner)));
    }

    String name; // Name of the generic parameter for this condition
    Type type;

    size_t index;
    OwnPtr<ImplCondition> inner;

    Result is_satisfied(quart::Type*);

private:
    ImplCondition(String name, Type type, size_t index, OwnPtr<ImplCondition> inner) : name(move(name)), type(type), index(index), inner(move(inner)) {}
};

class Impl {
public:
    static OwnPtr<Impl> create(Type* underlying_type, RefPtr<Scope> scope) {
        return OwnPtr<Impl>(new Impl(underlying_type, scope));
    }

    static OwnPtr<Impl> create(RefPtr<Scope> parent_scope, ast::TypeExpr* type, ast::BlockExpr* body, Vector<OwnPtr<ImplCondition>> conditions) {
        return OwnPtr<Impl>(new Impl(parent_scope, type, body, move(conditions)));
    }

    Type* underlying_type() const { return m_underlying_type; }
    RefPtr<Scope> scope() const { return m_scope; }

    RefPtr<Scope> parent_scope() const { return m_parent_scope; }

    ast::TypeExpr* type() const { return m_type; }
    ast::BlockExpr* body() const { return m_body; }

    bool is_generic() const { return m_type != nullptr; }

    ErrorOr<RefPtr<Scope>> make(State&, Type*);

private:
    Impl(Type* underlying_type, RefPtr<Scope> scope) : m_underlying_type(underlying_type), m_scope(move(scope)), m_parent_scope(scope->parent()) {}
    Impl(
        RefPtr<Scope> parent_scope, ast::TypeExpr* type, ast::BlockExpr* body, Vector<OwnPtr<ImplCondition>> conditions
    ) : m_parent_scope(move(parent_scope)), m_conditions(move(conditions)), m_type(type), m_body(body) {}

    Type* m_underlying_type = nullptr;
    RefPtr<Scope> m_scope = nullptr;

    RefPtr<Scope> m_parent_scope = nullptr;

    Vector<OwnPtr<ImplCondition>> m_conditions;
    HashMap<Type*, RefPtr<Scope>> m_impls;

    ast::TypeExpr* m_type = nullptr;
    ast::BlockExpr* m_body = nullptr;
};

}