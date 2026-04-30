#include <quart/codegen/codegen.h>

#include <quart/codegen/llvm/codegen.h>
#include <quart/codegen/x86_64/codegen.h>

namespace quart {

OwnPtr<CodeGen> CodeGen::create(State& state, CodeGenType type, String module) {
    switch (type) {
        case CodeGenType::LLVM:
            return OwnPtr<CodeGen>(new llvm::LLVMCodeGen(state, move(module)));
        case CodeGenType::x86_64:
            return OwnPtr<CodeGen>(new x86_64::x86_64CodeGen(state, move(module)));
        default:
            return nullptr;
    }
}

}