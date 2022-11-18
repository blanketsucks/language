#ifndef _JIT_H
#define _JIT_H

#include "llvm.h"
#include "compiler.h"
#include "utils.h"

namespace jit {

void checkError(llvm::Error error);

void ExitOnError(llvm::Error error);
template<typename T> T ExitOnError(llvm::Expected<T> &&error) {
    jit::checkError(error.takeError());
    return std::move(*error);
}

template<typename T> T ExitOnError(llvm::Expected<T&> &&error) {
    jit::checkError(error.takeError());
    return *error;
}

class ProtonJIT {
public:
    typedef int EntryFunction(int, char**);
    typedef void ErrorReporter(llvm::Error);

    template<typename T> static llvm::JITEvaluatedSymbol create_symbol_from_pointer(T* ptr) {
        return llvm::JITEvaluatedSymbol(llvm::pointerToJITTargetAddress(ptr), llvm::JITSymbolFlags());
    }

    ProtonJIT(std::string filename, utils::Ref<llvm::Module> module, utils::Ref<llvm::LLVMContext> context);

    llvm::orc::JITDylib& dylib() const;
    llvm::orc::SymbolStringPtr mangle(std::string name);

    void set_error_reporter(ErrorReporter* callback);

    llvm::orc::SymbolMap getSymbolMap() const;

    template<typename T> void define(std::string name, T* ptr) {
        this->symbols[this->mangle(name)] = ProtonJIT::create_symbol_from_pointer<T>(ptr);
    }

    template<typename T> T lookup(std::string name) {
        auto symbol = jit::ExitOnError(this->jit->lookup(name));
        return (T)symbol.getAddress();
    }

    void dump() const;

    int run(int argc = 0, char** argv = nullptr);
private:
    std::string filename;

    utils::Ref<llvm::orc::LLJIT> jit;
    llvm::orc::ThreadSafeModule module;

    llvm::orc::SymbolMap symbols;
};

}

#endif