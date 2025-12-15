#include <quart/bytecode/pass.h>
#include <quart/language/functions.h>

#include <quart/bytecode/passes/eliminate_unreachable_blocks.h>

namespace quart::bytecode {

void Pass::run(Function* function) {
    for (auto& block : function->basic_blocks()) {
        this->on_block(block);
    }
}

void Pass::on_block(BasicBlock* block) {
    for (auto& instruction : block->instructions()) {
        this->on_instruction(instruction.get());
    }
}

PassManager::~PassManager() {
    for (auto& pass : m_passes) {
        pass->finalize();
    }
}

PassManager PassManager::create_default() {
    PassManager manager;
    manager.add_pass<bytecode::EliminateUnreachableBlocksPass>();

    return manager;
}

void PassManager::add_pass(OwnPtr<Pass> pass) {
    m_passes.push_back(move(pass));
}

void PassManager::run(Function* function) {
    for (auto& pass : m_passes) {
        pass->run(function);
    }
}

}