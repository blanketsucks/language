#ifndef _PARSER_H
#define _PARSER_H

#include "ast.h"
#include "types.h"
#include "tokens.hpp"

#include <map>

struct Context {
    bool is_inside_function = false;
    StructType* current_struct = nullptr;
};

class Parser {
public:
    Parser(std::vector<Token> tokens);

    void error(const std::string& message);
    void end();

    Token next();
    Token peek();

    int get_token_precendence();
    Type* get_type(std::string name);

    std::unique_ptr<ast::BlockExpr> parse_block();
    std::unique_ptr<ast::PrototypeExpr> parse_prototype();
    std::unique_ptr<ast::FunctionExpr> parse_function();
    std::unique_ptr<ast::IfExpr> parse_if_statement();
    std::unique_ptr<ast::StructExpr> parse_struct();
    std::unique_ptr<ast::Expr> parse_variable_definition(bool is_const = false);
    std::unique_ptr<ast::NamespaceExpr> parse_namespace();

    std::unique_ptr<ast::Program> statements();
    std::unique_ptr<ast::Expr> statement();
    std::unique_ptr<ast::Expr> expr(bool semicolon = true);
    std::unique_ptr<ast::Expr> binary(int prec, std::unique_ptr<ast::Expr> left);
    std::unique_ptr<ast::Expr> unary();
    std::unique_ptr<ast::Expr> call();
    std::unique_ptr<ast::Expr> factor();

private:
    int index;
    Token current;
    Context context;
    std::vector<Token> tokens;
    std::map<TokenType, int> precedences;
    std::map<std::string, Type*> types;
};

#endif
