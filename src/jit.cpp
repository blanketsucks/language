#include <quart/jit.h>

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
    const std::string& filename,
    const std::string& entry,
    utils::Scope<llvm::Module> module, 
    utils::Scope<llvm::LLVMContext> context
) {
    this->filename = filename;
    this->entry = entry;

    this->jit = jit::ExitOnError(llvm::orc::LLJITBuilder().create());
    this->module = llvm::orc::ThreadSafeModule(std::move(module), std::move(context));

    auto& dylib = this->jit->getMainJITDylib();
    const llvm::DataLayout& layout = this->jit->getDataLayout();

    auto generator = llvm::cantFail(
        llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(layout.getGlobalPrefix())
    );
    
    dylib.addGenerator(std::move(generator));

    this->set_error_reporter(jit::checkError);
}

llvm::orc::JITDylib& jit::QuartJIT::dylib() const {
    return this->jit->getMainJITDylib();
}

llvm::orc::SymbolMap jit::QuartJIT::get_symbol_map() const {
    return this->symbols;
}

llvm::orc::SymbolStringPtr jit::QuartJIT::mangle(const std::string& name) {
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

    llvm::cantFail(dylib.define(llvm::orc::absoluteSymbols(symbols)));
    jit::ExitOnError(this->jit->addIRModule(std::move(module)));

    if (auto ctor = this->jit->lookup("__global_constructors_init")) {
        ((CtorFunction*)ctor->getAddress())();
    }

    auto entry = this->lookup<EntryFunction*>(this->entry);
    return entry(argc, argv);
}