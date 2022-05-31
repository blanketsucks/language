#ifndef _PARSER_H
#define _PARSER_H

#include "ast.h"
#include "types.hpp"
#include "tokens.hpp"

#include <map>

#define TODO(x) std::cout << "TODO: " << x << '\n'; exit(1);

struct Context {
    bool is_inside_function = false;
};

class Parser {
public:
    Parser(std::vector<Token> tokens);

    void error(const std::string& message);
    void end();

    Token next();
    Token peek();

    int get_token_precendence();
    Type get_type(std::string name);

    std::unique_ptr<ast::PrototypeExpr> parse_prototype();
    std::unique_ptr<ast::FunctionExpr> parse_function();

    std::unique_ptr<ast::Program> statements();

    std::unique_ptr<ast::Expr> statement();
    std::unique_ptr<ast::Expr> expr(bool expect_semicolon = true);
    std::unique_ptr<ast::Expr> binary(int prec, std::unique_ptr<ast::Expr> left);
    std::unique_ptr<ast::Expr> factor();

private:
    int index;
    Token current;
    Context context;
    std::vector<Token> tokens;
    std::map<TokenType, int> precedences;
    std::map<std::string, Type> types;
};

#endif
