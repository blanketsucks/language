#define _DEBUG_MODULE 1

#include "src/lexer.h"
#include "src/parser.h"
#include "src/types.h"
#include "src/visitor.h"

template<typename T> void error(T& message) {
    std::cerr << message << std::endl;
}

template<typename T, typename... Args> void error(
    Location* start, Location* end, T& message, Args... args
) {
    std::cerr << message << " ";
    error(args...);
} 

int main() {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    std::ifstream file("test.txt");

    Lexer lexer(file, "test.txt");
    std::vector<Token> tokens = lexer.lex();

    std::unique_ptr<ast::Program> program = Parser(tokens).statements();
    
    Visitor visitor("test");
    visitor.visit(std::move(program));

    // Compile to object code

    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    std::string error;

    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        std::cerr << "Failed to create target: " << error << std::endl;
        return 1;
    }

    std::string cpu = "generic";
    std::string features = "";

    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>();

    llvm::TargetMachine* target_machine = target->createTargetMachine(target_triple, cpu, features, options, reloc);
    visitor.module->setDataLayout(target_machine->createDataLayout());
    visitor.module->setTargetTriple(target_triple);

    std::string filename = "output.o";
    std::error_code ec;

    llvm::raw_fd_ostream dest(filename, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Could not open file. " << ec.message() << std::endl;
        return 1;
    }

    llvm::legacy::PassManager pass;
    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, llvm::CGFT_ObjectFile)) {
        std::cerr << "Target machine can't emit a file of this type" << std::endl;
        return 1;
    }

#if _DEBUG_MODULE
    visitor.dump(llvm::outs());
#endif

    pass.run(*visitor.module);
    dest.flush();
} 