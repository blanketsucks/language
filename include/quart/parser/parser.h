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

    Token const& peek(u32 offset = 1) const;
    Token& peek(u32 offset = 1);

    Token expect(TokenKind type, llvm::StringRef value);

    bool is_valid_attribute(llvm::StringRef name);

    int get_token_precendence();

    AttributeHandler::Result handle_expr_attributes(const ast::Attributes& attrs);

    bool is_upcoming_constructor(ast::Expr const& previous) const;
    
    OwnPtr<ast::TypeExpr> parse_type();

    OwnPtr<ast::BlockExpr> parse_block();
    std::pair<std::vector<ast::Argument>, bool> parse_arguments(); // Returns a pair of arguments and whether or not the arguments are variadic
    OwnPtr<ast::PrototypeExpr> parse_prototype(
        ast::ExternLinkageSpecifier linkage, bool with_name, bool is_operator
    );
    OwnPtr<ast::Expr> parse_function_definition(
        ast::ExternLinkageSpecifier linkage = ast::ExternLinkageSpecifier::None, 
        bool is_operator = false
    );

    std::vector<ast::GenericParameter> parse_generic_parameters();
    ast::ExprList<ast::TypeExpr> parse_generic_arguments();

    // The difference between this and parse_function_definition is that this one can never return a prototype forcing an 
    // implementation to be provided
    OwnPtr<ast::FunctionExpr> parse_function();

    OwnPtr<ast::IfExpr> parse_if_statement();
    OwnPtr<ast::StructExpr> parse_struct();
    OwnPtr<ast::Expr> parse_variable_definition(bool is_const = false);
    OwnPtr<ast::Expr> parse_extern(ast::ExternLinkageSpecifier linkage);
    OwnPtr<ast::Expr> parse_extern_block();
    OwnPtr<ast::EnumExpr> parse_enum();
    OwnPtr<ast::TypeAliasExpr> parse_type_alias();
    OwnPtr<ast::Expr> parse_anonymous_function();
    OwnPtr<ast::MatchExpr> parse_match_expr();

    OwnPtr<ast::CallExpr> parse_call(
        Span span, OwnPtr<ast::Expr> callee
    );

    // Parses `foo::bar::baz` into a deque of strings
    Path parse_path(llvm::Optional<std::string> name = llvm::None);

    OwnPtr<ast::Expr> parse_immediate_binary_op(OwnPtr<ast::Expr> right, OwnPtr<ast::Expr> left, TokenKind op);
    OwnPtr<ast::Expr> parse_immediate_unary_op(OwnPtr<ast::Expr> expr, TokenKind op);

    ast::Attributes parse_attributes();

    std::vector<OwnPtr<ast::Expr>> parse();
    std::vector<OwnPtr<ast::Expr>> statements();
    OwnPtr<ast::Expr> statement();
    OwnPtr<ast::Expr> expr(bool semicolon = true);
    OwnPtr<ast::Expr> binary(int prec, OwnPtr<ast::Expr> left);
    OwnPtr<ast::Expr> unary();
    OwnPtr<ast::Expr> call();
    OwnPtr<ast::Expr> attr(Span start, OwnPtr<ast::Expr> expr);
    OwnPtr<ast::Expr> element(Span start, OwnPtr<ast::Expr> expr);
    OwnPtr<ast::Expr> factor();

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