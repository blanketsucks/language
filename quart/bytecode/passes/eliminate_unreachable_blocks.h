#pragma once

#include <quart/common.h>
#include <quart/bytecode/pass.h>

namespace quart::bytecode {

class EliminateUnreachableBlocksPass : public Pass {
public:
    struct FunctionUse {
        Set<Function*> callers;
        size_t count() const { return callers.size(); }
    };

    EliminateUnreachableBlocksPass() = default;

    void finalize() override;

    void run(Function*) override;
    void on_instruction(Instruction*) override;

private:
    bool is_called(Function* function) const;

    HashMap<BasicBlock*, size_t> m_block_use_count;
    HashMap<Function*, FunctionUse> m_function_use_count;
};

}