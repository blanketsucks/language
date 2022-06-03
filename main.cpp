#include "src/lexer.h"
#include "src/parser.h"
#include "src/types.h"
#include "src/visitor.h"

int main() {
    std::ifstream file("test.txt");

    Lexer lexer(file, "test.txt");
    std::vector<Token> tokens = lexer.lex();

    std::unique_ptr<ast::Program> program = Parser(tokens).statements();
    
    Visitor visitor("test");
    visitor.visit(std::move(program));

    visitor.dump(llvm::errs());

    Type type = Type::LongLong;
    std::cout << type.is_numeric() << std::endl;
}