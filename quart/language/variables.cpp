#include <quart/language/variables.h>
#include <quart/language/state.h>

namespace quart {

void Variable::emit(State& state, bytecode::Register dst) {
    if (m_flags & Constant) {
        state.emit<bytecode::GetGlobal>(dst, m_index);
    } else {
        state.emit<bytecode::GetLocal>(dst, m_index);
    }

    state.set_register_type(dst, m_type);
}

}