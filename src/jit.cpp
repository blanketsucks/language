#include "jit.h"

void jit::checkError(llvm::Error error) {
    if (error) {
        llvm::outs() << utils::color(WHITE, "quart: ");
        llvm::outs() << utils::color(RED, "error: ") << error << '\n';

        exit(1);
    }
}

void jit::ExitOnError(llvm::Error error) {
    jit::checkError(std::move(error));
}

jit::QuartJIT::QuartJIT(
    std::string filename, utils::Ref<llvm::Module> module, utils::Ref<llvm::LLVMContext> context
) {
    this->filename = filename;
    this->jit = jit::ExitOnError(llvm::orc::LLJITBuilder().create());
    this->module = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));

    auto& dylib = this->jit->getMainJITDylib();
    const llvm::DataLayout& layout = this->jit->getDataLayout();

    dylib.addGenerator(llvm::cantFail(llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(layout.getGlobalPrefix())));
    
    // Add a default error reporter function
    this->set_error_reporter(jit::checkError);
}

llvm::orc::JITDylib& jit::QuartJIT::dylib() const {
    return this->jit->getMainJITDylib();
}

llvm::orc::SymbolMap jit::QuartJIT::getSymbolMap() const {
    return this->symbols;
}

llvm::orc::SymbolStringPtr jit::QuartJIT::mangle(std::string name) {
    return this->jit->mangleAndIntern(name);
}

void jit::QuartJIT::set_error_reporter(ErrorReporter* callback) {
    auto& session = this->jit->getExecutionSession();
    session.setErrorReporter(callback);
}

void jit::QuartJIT::dump() const {
    auto& dylib = this->jit->getMainJITDylib();
    dylib.dump(llvm::outs());
}

int jit::QuartJIT::run(int argc, char** argv) {
    auto& dylib = this->jit->getMainJITDylib();

    llvm::cantFail(dylib.define(absoluteSymbols(symbols)));
    jit::ExitOnError(this->jit->addIRModule(std::move(module)));

    // Make sure global constructors are properly called
    if (auto ctor = this->jit->lookup("__global_constructors_init")) {
        ((CtorFunction*)ctor->getAddress())();
    }

    auto entry = this->lookup<EntryFunction*>("main");
    return entry(argc, argv);
}