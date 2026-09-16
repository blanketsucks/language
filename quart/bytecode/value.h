#pragma once

#include <quart/common.h>
#include <quart/language/types.h>

#include <ostream>

namespace quart::bytecode {

class Instruction;

class Value {
public:
    virtual ~Value() = default;

    NO_COPY(Value);
    NO_MOVE(Value);

    enum ValueID {
        Unknown = 0,
        ConstantID,
        InstructionID,
        FunctionID
    };

    Type* type() const { return m_type; }
    ValueID id() const { return m_id; }

    Vector<Instruction*> const& users() const { return m_users; }
    void add_user(Instruction* inst) { m_users.push_back(inst); }

    bool is_used() const { return !m_users.empty(); }
    
    String const& value_name() const { return m_name; }
    void set_value_name(String name) { m_name = move(name); }

    void set_type(Type* type) { m_type = type; }

    virtual void print(std::ostream&) const = 0;
    void dump() const;

protected:
    Value(Type* type, ValueID id);

private:
    Type* m_type = nullptr;
    ValueID m_id;

    Vector<Instruction*> m_users;
    String m_name;
};

}