#pragma once

#include <quart/lexer/location.h>
#include <quart/lexer/tokens.h>
#include <quart/parser/ast.h>

#include <llvm/ADT/Optional.h>

#include <map>

namespace quart {

struct Path {
    std::string name;
    std::deque<std::string> segments;
};

class Parser {
public:
    typedef Attribute (*AttributeFunc)(Parser&);
    
    Parser(std::vector<Token> tokens);

    void end();

    Token next();
    Token rewind(u32 n = 1);

    Token peek(u32 offset = 1);

    Token expect(TokenKind type, llvm::StringRef value);

    bool is_valid_attribute(llvm::StringRef name);

    int get_token_precendence();

    AttributeHandler::Result handle_expr_attributes(const ast::Attributes& attrs);
    
    std::unique_ptr<ast::TypeExpr> parse_type();

    std::unique_ptr<ast::BlockExpr> parse_block();
    std::pair<std::vector<ast::Argument>, bool> parse_arguments(); // Returns a pair of arguments and whether or not the arguments are variadic
    std::unique_ptr<ast::PrototypeExpr> parse_prototype(
        ast::ExternLinkageSpecifier linkage, bool with_name, bool is_operator
    );
    std::unique_ptr<ast::Expr> parse_function_definition(
        ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None, 
        bool is_operator = false
    );

    std::vector<ast::GenericParameter> parse_generic_parameters();
    ast::ExprList<ast::TypeExpr> parse_generic_arguments();

    // The difference between this and parse_function_definition is that this one can never return a prototype forcing an 
    // implementation to be provided
    std::unique_ptr<ast::FunctionExpr> parse_function();

    std::unique_ptr<ast::IfExpr> parse_if_statement();
    std::unique_ptr<ast::StructExpr> parse_struct();
    std::unique_ptr<ast::Expr> parse_variable_definition(bool is_const = false);
    std::unique_ptr<ast::Expr> parse_extern(ast::ExternLinkageSpecifier linkage);
    std::unique_ptr<ast::Expr> parse_extern_block();
    std::unique_ptr<ast::EnumExpr> parse_enum();
    std::unique_ptr<ast::TypeAliasExpr> parse_type_alias();
    std::unique_ptr<ast::Expr> parse_anonymous_function();
    std::unique_ptr<ast::MatchExpr> parse_match_expr();

    // Parses `foo::bar::baz` into a deque of strings
    Path parse_path(llvm::Optional<std::string> name = llvm::None);

    std::unique_ptr<ast::Expr> parse_immediate_binary_op(std::unique_ptr<ast::Expr> right, std::unique_ptr<ast::Expr> left, TokenKind op);
    std::unique_ptr<ast::Expr> parse_immediate_unary_op(std::unique_ptr<ast::Expr> expr, TokenKind op);

    ast::Attributes parse_attributes();

    std::vector<std::unique_ptr<ast::Expr>> parse();
    std::vector<std::unique_ptr<ast::Expr>> statements();
    std::unique_ptr<ast::Expr> statement();
    std::unique_ptr<ast::Expr> expr(bool semicolon = true);
    std::unique_ptr<ast::Expr> binary(int prec, std::unique_ptr<ast::Expr> left);
    std::unique_ptr<ast::Expr> unary();
    std::unique_ptr<ast::Expr> call();
    std::unique_ptr<ast::Expr> attr(Span start, std::unique_ptr<ast::Expr> expr);
    std::unique_ptr<ast::Expr> element(Span start, std::unique_ptr<ast::Expr> expr);
    std::unique_ptr<ast::Expr> factor();

    size_t index;
    Token current;

    bool is_inside_function = false;
    bool is_inside_loop = false;
    bool is_inside_struct = false;
    bool self_usage_allowed = false;

    std::vector<Token> tokens;

    std::map<llvm::StringRef, AttributeFunc> attributes;
};

}