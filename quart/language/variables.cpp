#include <quart/language/variables.h>
#include <quart/language/state.h>

namespace quart {

bytecode::Instruction* Variable::emit(State& state) {
    if (m_flags & Global) {
        return state.emit<bytecode::GetGlobal>(m_type, m_index);
    } else {
        return state.emit<bytecode::GetLocal>(m_type, m_index);
    }
}

}