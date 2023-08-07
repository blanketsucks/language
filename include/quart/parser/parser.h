#pragma once

#include <quart/lexer/location.h>
#include <quart/lexer/tokens.h>
#include <quart/parser/ast.h>
#include <quart/utils/pointer.h>

#include <llvm/ADT/Optional.h>

#include <map>

struct Path {
    std::string name;
    std::deque<std::string> segments;
};

class Parser {
public:
    typedef Attribute (*AttributeHandler)(Parser&);

    static std::map<std::string, ast::BuiltinType> TYPES;
    
    Parser(std::vector<Token> tokens);

    void end();

    Token next();
    Token rewind(uint32_t n = 1);

    Token peek(uint32_t offset = 1);

    Token expect(TokenKind type, const std::string& value);

    bool is_valid_attribute(const std::string& name);

    int get_token_precendence();
    
    utils::Scope<ast::TypeExpr> parse_type();

    utils::Scope<ast::BlockExpr> parse_block();
    std::pair<std::vector<ast::Argument>, bool> parse_arguments(); // Returns a pair of arguments and whether or not the arguments are variadic
    utils::Scope<ast::PrototypeExpr> parse_prototype(
        ast::ExternLinkageSpecifier linkage, bool with_name, bool is_operator
    );
    utils::Scope<ast::Expr> parse_function_definition(
        ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None, 
        bool is_operator = false
    );

    // The difference between this and parse_function_definition is that this one can never return a prototype forcing an 
    // implementation to be provided
    utils::Scope<ast::FunctionExpr> parse_function();

    utils::Scope<ast::IfExpr> parse_if_statement();
    utils::Scope<ast::StructExpr> parse_struct();
    utils::Scope<ast::Expr> parse_variable_definition(bool is_const = false);
    utils::Scope<ast::NamespaceExpr> parse_namespace();
    utils::Scope<ast::Expr> parse_extern(ast::ExternLinkageSpecifier linkage);
    utils::Scope<ast::Expr> parse_extern_block();
    utils::Scope<ast::EnumExpr> parse_enum();
    utils::Scope<ast::TypeAliasExpr> parse_type_alias();
    utils::Scope<ast::Expr> parse_anonymous_function();
    utils::Scope<ast::MatchExpr> parse_match_expr();

    // Parses `foo::bar::baz` into a deque of strings
    Path parse_path(llvm::Optional<std::string> name = llvm::None);

    utils::Scope<ast::Expr> parse_immediate_binary_op(utils::Scope<ast::Expr> right, utils::Scope<ast::Expr> left, TokenKind op);
    utils::Scope<ast::Expr> parse_immediate_unary_op(utils::Scope<ast::Expr> expr, TokenKind op);

    ast::Attributes parse_attributes();

    std::vector<utils::Scope<ast::Expr>> parse();
    std::vector<utils::Scope<ast::Expr>> statements();
    utils::Scope<ast::Expr> statement();
    utils::Scope<ast::Expr> expr(bool semicolon = true);
    utils::Scope<ast::Expr> binary(int prec, utils::Scope<ast::Expr> left);
    utils::Scope<ast::Expr> unary();
    utils::Scope<ast::Expr> call();
    utils::Scope<ast::Expr> attr(Span start, utils::Scope<ast::Expr> expr);
    utils::Scope<ast::Expr> element(Span start, utils::Scope<ast::Expr> expr);
    utils::Scope<ast::Expr> factor();

    size_t index;
    Token current;

    bool is_inside_function = false;
    bool is_inside_loop = false;
    bool is_inside_struct = false;
    bool self_usage_allowed = false;

    std::vector<Token> tokens;

    std::map<std::string, AttributeHandler> attributes;
};

