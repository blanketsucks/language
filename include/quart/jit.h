#pragma once

#include <quart/llvm.h>
#include <quart/compiler.h>

namespace quart {

namespace jit {

struct StaticLibrary {
    std::string name;
};

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

class QuartJIT {
public:
    typedef int EntryFunction(int, char**);
    typedef void CtorFunction(void);
    typedef void ErrorReporter(llvm::Error);

    template<typename T> static llvm::JITEvaluatedSymbol create_symbol_from_pointer(T* ptr) {
        return llvm::JITEvaluatedSymbol(llvm::pointerToJITTargetAddress(ptr), llvm::JITSymbolFlags());
    }

    QuartJIT(
        const std::string& filename,
        const std::string& entry,
        std::unique_ptr<llvm::Module> module, 
        std::unique_ptr<llvm::LLVMContext> context
    );

    llvm::orc::JITDylib& dylib() const;
    llvm::orc::SymbolStringPtr mangle(const std::string& name);

    void set_error_reporter(ErrorReporter* callback);

    llvm::orc::SymbolMap get_symbol_map() const;

    template<typename T> void define(const std::string& name, T* ptr) {
        this->symbols[this->mangle(name)] = QuartJIT::create_symbol_from_pointer<T>(ptr);
    }

    template<typename T> T lookup(const std::string& name) {
        auto symbol = jit::ExitOnError(this->jit->lookup(name));
        return (T)symbol.getAddress();
    }

    void dump() const;

    int run(int argc = 0, char** argv = nullptr);
private:
    std::string filename;
    std::string entry;

    std::unique_ptr<llvm::orc::LLJIT> jit;
    llvm::orc::ThreadSafeModule module;

    llvm::orc::SymbolMap symbols;
};

}

}