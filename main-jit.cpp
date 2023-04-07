#include <quart/lexer.h>
#include <quart/parser.h>
#include <quart/visitor.h>
#include <quart/jit.h>

int main(int argc, char** argv) {
    Compiler::init();

    // TODO: Proper compiler interface
    // TODO: Incorporate into the main executable
    if (argc < 2) {
        Compiler::error("No input file specified");
        return 1;
    }

    std::string filename = argv[1];

    utils::fs::Path path(filename);
    if (!path.exists()) {
        Compiler::error("File not found '{0}'", filename);
        return 1;
    }

    Lexer lexer(path);
    auto tokens = lexer.lex();

    Parser parser(tokens);
    auto ast = parser.parse();

    CompilerOptions options;
    options.entry = "main";

    Visitor visitor(filename, options);
    visitor.visit(std::move(ast));

    auto& entry = visitor.global_scope->functions["main"];
    if (!entry) {
        Compiler::error("Missing main entry point function"); exit(1);
    }

    if (entry->args.size() > 2) {
        Compiler::error("Main entry point function takes no more than 2 arguments"); exit(1);
    }

    // Apparently, we can only access the global ctor function only if it has the external linkage
    visitor.create_global_constructors(llvm::Function::ExternalLinkage);
    visitor.finalize();

    jit::QuartJIT jit = jit::QuartJIT(
        filename, std::move(visitor.module), std::move(visitor.context)
    );
    
    int code = jit.run(argc - 1, argv + sizeof(char));

    llvm::llvm_shutdown();
    return code;
}