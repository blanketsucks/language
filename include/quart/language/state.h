#pragma once

#include <quart/bytecode/generator.h>
#include <quart/language/type_registry.h>
#include <quart/language/scopes.h>

namespace quart {

class State {
public:
    State() = default;

    bytecode::Generator& generator() { return m_generator; }
    TypeRegistry& types() { return *m_type_registry; }

    Scope* scope() const { return m_current_scope; }
    void set_current_scope(Scope* scope) { m_current_scope = scope; }

    ErrorOr<Scope*> resolve_scope_path(Span, const Path&);

    size_t register_count() { return m_generator.register_count(); }

    bytecode::Register allocate_register();
    void set_register_type(bytecode::Register reg, quart::Type* type);

    quart::Type* type(bytecode::Operand) const;

    template<typename T, typename... Args>
    inline T* emit(Args&&... args) {
        return m_generator.emit<T>(std::forward<Args>(args)...);
    }

private:
    bytecode::Generator m_generator;
    OwnPtr<TypeRegistry> m_type_registry;

    Vector<quart::Type*> m_registers;

    Scope* m_current_scope = nullptr;

    HashMap<quart::Type*, RefPtr<Struct>> m_all_structs;
    HashMap<String, RefPtr<Function>> m_all_functions;

    HashMap<String, RefPtr<Module>> m_modules;

};

}