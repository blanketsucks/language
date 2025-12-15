#include <quart/bytecode/passes/eliminate_unreachable_blocks.h>
#include <quart/language/functions.h>

namespace quart::bytecode {

bool EliminateUnreachableBlocksPass::is_called(Function* function) const {
    auto iterator = m_function_use_count.find(function);
    if (iterator == m_function_use_count.end()) {
        return false;
    }

    auto& use = iterator->second;
    if (use.callers.size() == 1) {
        Function* caller = *use.callers.begin();
        if (caller == function) {
            return false;
        }

        return is_called(caller);
    }

    for (Function* caller : use.callers) {
        if (function == caller) {
            continue;
        }

        if (caller->is_main() || is_called(caller)) {
            return true;
        }
    }

    return false;
}

void EliminateUnreachableBlocksPass::finalize() {
    for (auto& [function, use] : m_function_use_count) {
        if (function->is_main()) {
            continue;
        }

        if (use.callers.empty()) {
            function->set_used(false);
            continue;
        }

        if (use.callers.size() == 1) {
            function->set_used(!use.callers.contains(function));
            continue;
        }

        bool used = false;
        for (Function* caller : use.callers) {
            if (caller == function) {
                continue;
            }

            if (caller->is_main() || this->is_called(caller)) {
                used = true;
                break;
            }
        }

        function->set_used(used);
    }
}

void EliminateUnreachableBlocksPass::run(Function* function) {
    for (auto& block : function->basic_blocks()) {
        this->on_block(const_cast<BasicBlock*>(block));
    }

    Vector<BasicBlock*> unreachable_blocks;
    for (auto& block : function->basic_blocks()) {
        if (block == function->entry_block()) {
            continue;
        }

        size_t use_count = m_block_use_count[block];
        if (use_count == 0) {
            unreachable_blocks.push_back(block);
        }
    }

    for (auto* block : unreachable_blocks) {
        function->remove_block(block);
    }
}

void EliminateUnreachableBlocksPass::on_instruction(Instruction* instruction) {
    switch (instruction->type()) {
        case Instruction::Jump: {
            auto* jump = dynamic_cast<Jump*>(instruction);
            m_block_use_count[jump->target()]++;

            break;
        }
        case Instruction::JumpIf: {
            auto* jump_if = dynamic_cast<JumpIf*>(instruction);

            m_block_use_count[jump_if->true_target()]++;
            m_block_use_count[jump_if->false_target()]++;

            break;
        }
        case Instruction::GetFunction: {
            // TODO: Track if the function is used after this instruction.
            auto* get_function = dynamic_cast<GetFunction*>(instruction);
            auto& use = m_function_use_count[get_function->function()];

            use.callers.insert(instruction->parent()->parent());
            break;
        }
        default:
            break;
    }
}

}