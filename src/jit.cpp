#include "jit.h"
#include "utils.h"

void jit::checkError(llvm::Error error) {
    if (error) {
        // llvm::outs() << utils::fmt::format("{bold|white}", "proton: ");
        // llvm::outs() << utils::fmt::format("{bold|red}", "error: ") << error << '\n';

        exit(1);
    }
}

void jit::ExitOnError(llvm::Error error) {
    jit::checkError(std::move(error));
}

jit::ProtonJIT::ProtonJIT(
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

llvm::orc::JITDylib& jit::ProtonJIT::dylib() const {
    return this->jit->getMainJITDylib();
}

llvm::orc::SymbolMap jit::ProtonJIT::getSymbolMap() const {
    return this->symbols;
}

llvm::orc::SymbolStringPtr jit::ProtonJIT::mangle(std::string name) {
    return this->jit->mangleAndIntern(name);
}

void jit::ProtonJIT::set_error_reporter(ErrorReporter* callback) {
    auto& session = this->jit->getExecutionSession();
    session.setErrorReporter(callback);
}

void jit::ProtonJIT::dump() const {
    auto& dylib = this->jit->getMainJITDylib();
    dylib.dump(llvm::outs());
}

int jit::ProtonJIT::run(int argc, char** argv) {
    auto& dylib = this->jit->getMainJITDylib();

    llvm::cantFail(dylib.define(absoluteSymbols(symbols)));
    jit::ExitOnError(this->jit->addIRModule(std::move(module)));

    auto entry = this->lookup<EntryFunction*>("main");
    return entry(argc, argv);
}