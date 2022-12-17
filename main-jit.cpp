#include "lexer/lexer.h"
#include "parser/parser.h"
#include "preprocessor.h"
#include "visitor.h"

#include "jit.h"

int main(int argc, char** argv) {
    Compiler::init();

    // TODO: Proper compiler interface
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <file>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];

    utils::filesystem::Path path(filename);
    if (!path.exists()) {
        Compiler::error("File not found '{0}'", filename);
        return 1;
    }

    std::fstream file(filename, std::ios::in);
    Lexer lexer(file, filename);

    Preprocessor preprocessor(lexer.lex(), {"lib/"});
    auto tokens = preprocessor.process();

    Parser parser(tokens);
    auto ast = parser.parse();

    Visitor visitor(filename, "main", true);
    visitor.visit(std::move(ast));

    auto entry = visitor.global_scope->functions["main"];
    if (!entry) {
        Compiler::error("Missing main entry point function");
    }

    if (entry->args.size() > 2) {
        Compiler::error("Main entry point function takes no more than 2 arguments");
    }

    // Apparently, we can only access the global ctor function only if it has the external linkage
    visitor.create_global_constructors(llvm::Function::ExternalLinkage);
    visitor.finalize();

    jit::QuartJIT jit = jit::QuartJIT(filename, std::move(visitor.module), std::move(visitor.context));
    int code = jit.run(argc - 1, argv + sizeof(char));

    llvm::llvm_shutdown();
    return code;
}