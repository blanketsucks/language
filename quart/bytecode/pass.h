#pragma once

#include <quart/common.h>
#include <quart/bytecode/instruction.h>
#include <quart/bytecode/basic_block.h>

namespace quart::bytecode {

class Pass {
public:
    virtual ~Pass() = default;
    Pass() = default;

    NO_COPY(Pass)
    NO_MOVE(Pass)

    virtual void run(Function*);
    virtual void finalize() {}

    virtual void on_block(BasicBlock*);
    virtual void on_instruction(Instruction*) = 0;
};

class PassManager {
public:
    PassManager() = default;
    ~PassManager();

    DEFAULT_COPY(PassManager)
    DEFAULT_MOVE(PassManager)

    static PassManager create_default();

    void add_pass(OwnPtr<Pass> pass);

    template<typename T, typename... Args> requires(std::is_base_of_v<Pass, T>)
    void add_pass(Args&&... args) {
        m_passes.push_back(make<T>(std::forward<Args>(args)...));
    }

    void run(Function*);

private:
    Vector<OwnPtr<Pass>> m_passes;
};

}