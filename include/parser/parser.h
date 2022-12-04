#ifndef _PARSER_H
#define _PARSER_H

#include "parser/ast.h"
#include "lexer/tokens.h"
#include "utils.h"

#include "llvm/ADT/Optional.h"

#include <map>

class Parser {
public:
    Parser(std::vector<Token> tokens);

    void end();

    Token next();
    Token peek();

    Token expect(TokenKind type, std::string value);

    int get_token_precendence();
    
    utils::Ref<ast::TypeExpr> parse_type();

    utils::Ref<ast::BlockExpr> parse_block();
    utils::Ref<ast::PrototypeExpr> parse_prototype(ast::ExternLinkageSpecifier linkage, bool with_name);
    utils::Ref<ast::Expr> parse_function_definition(
        ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None
    );
    utils::Ref<ast::IfExpr> parse_if_statement();
    utils::Ref<ast::StructExpr> parse_struct();
    utils::Ref<ast::Expr> parse_variable_definition(bool is_const = false);
    utils::Ref<ast::NamespaceExpr> parse_namespace();
    utils::Ref<ast::Expr> parse_extern(ast::ExternLinkageSpecifier linkage);
    utils::Ref<ast::Expr> parse_extern_block();
    utils::Ref<ast::EnumExpr> parse_enum();
    utils::Ref<ast::TypeAliasExpr> parse_type_alias();

    utils::Ref<ast::Expr> parse_immediate_binary_op(utils::Ref<ast::Expr> right, utils::Ref<ast::Expr> left, TokenKind op);
    utils::Ref<ast::Expr> parse_immediate_unary_op(utils::Ref<ast::Expr> expr, TokenKind op);

    ast::Attributes parse_attributes();

    std::vector<utils::Ref<ast::Expr>> parse();
    std::vector<utils::Ref<ast::Expr>> statements();
    utils::Ref<ast::Expr> statement();
    utils::Ref<ast::Expr> expr(bool semicolon = true);
    utils::Ref<ast::Expr> binary(int prec, utils::Ref<ast::Expr> left);
    utils::Ref<ast::Expr> unary();
    utils::Ref<ast::Expr> call();
    utils::Ref<ast::Expr> attr(Location start, utils::Ref<ast::Expr> expr);
    utils::Ref<ast::Expr> element(Location start, utils::Ref<ast::Expr> expr);
    utils::Ref<ast::Expr> factor();

private:
    uint32_t index;
    Token current;

    bool is_inside_function = false;
    bool is_inside_loop = false;
    bool is_inside_struct = false;

    std::vector<Token> tokens;
    std::map<TokenKind, int> precedences;
    std::map<std::string, ast::BuiltinType> types;
};

#endif
