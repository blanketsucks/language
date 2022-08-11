#ifndef _PARSER_H
#define _PARSER_H

#include "types/include.h"
#include "parser/ast.h"
#include "lexer/tokens.h"
#include "utils.h"

#include <map>

struct Context {
    bool is_inside_function = false;
    StructType* current_struct = nullptr;
};

class Parser {
public:
    Parser(std::vector<Token> tokens);

    void free();

    template<typename T> T itoa(const std::string& str) {
        T result = 0;
        bool error = llvm::StringRef(str).getAsInteger(0, result);
        if (error) {
            ERROR(this->current.start, "Invalid integer literal");
        }

        return result;
    }

    void end();

    Token next();
    Token peek();

    Token expect(TokenType type, std::string value);

    int get_token_precendence();
    Type* parse_type(std::string name, bool should_error = true);
    std::map<std::string, Type*> get_types() { return this->types; }

    std::unique_ptr<ast::BlockExpr> parse_block();
    std::unique_ptr<ast::PrototypeExpr> parse_prototype(ast::ExternLinkageSpecifier linkage);
    std::unique_ptr<ast::Expr> parse_function_definition(
        ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None
    );
    std::unique_ptr<ast::IfExpr> parse_if_statement();
    std::unique_ptr<ast::StructExpr> parse_struct();
    std::unique_ptr<ast::Expr> parse_variable_definition(bool is_const = false);
    std::unique_ptr<ast::NamespaceExpr> parse_namespace();
    std::unique_ptr<ast::Expr> parse_extern(ast::ExternLinkageSpecifier linkage);
    std::unique_ptr<ast::BlockExpr> parse_extern_block();
    std::unique_ptr<ast::InlineAssemblyExpr> parse_inline_assembly();

    std::unique_ptr<ast::Expr> parse_immediate_binary_op(
        std::unique_ptr<ast::Expr> right, std::unique_ptr<ast::Expr> left, TokenType op
    );

    std::unique_ptr<ast::Expr> parse_immediate_unary_op(std::unique_ptr<ast::Expr> expr, TokenType op);

    ast::Attributes parse_attributes();

    std::vector<std::unique_ptr<ast::Expr>> statements();
    std::unique_ptr<ast::Expr> statement();
    std::unique_ptr<ast::Expr> expr(bool semicolon = true);
    std::unique_ptr<ast::Expr> binary(int prec, std::unique_ptr<ast::Expr> left);
    std::unique_ptr<ast::Expr> unary();
    std::unique_ptr<ast::Expr> call();
    std::unique_ptr<ast::Expr> attr(Location start, std::unique_ptr<ast::Expr> expr);
    std::unique_ptr<ast::Expr> element(Location start, std::unique_ptr<ast::Expr> expr);
    std::unique_ptr<ast::Expr> factor();

private:
    uint32_t index;
    Token current;
    Context context;
    std::vector<Token> tokens;
    std::map<TokenType, int> precedences;
    std::map<std::string, Type*> types;
    std::vector<Type*> _allocated_types;
};

#endif
