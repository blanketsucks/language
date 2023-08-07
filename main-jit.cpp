#include <quart/lexer.h>
#include <quart/parser.h>
#include <quart/visitor.h>
#include <quart/jit.h>
#include <quart/objects/scopes.h>

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
    options.input = path;

    Compiler compiler(options);
    int code = compiler.jit(argc - 1, argv + 1);
    
    Compiler::shutdown();
    return code;
}