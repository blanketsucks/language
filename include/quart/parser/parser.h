#pragma once

#include <quart/lexer/tokens.h>
#include <quart/parser/ast.h>

#include <map>

namespace quart {

// template<typename T>
// using ParseResult = ErrorOr<OwnPtr<T>>;

template<typename T>
class ParseResult : public ErrorOr<OwnPtr<T>> {
public:
    ParseResult() = default;

    ParseResult(OwnPtr<T> value) : ErrorOr<OwnPtr<T>>(move(value)) {}
    ParseResult(Error error) : ErrorOr<OwnPtr<T>>(move(error)) {}

    template<typename U> requires(std::is_base_of_v<T, U>)
    ParseResult(ParseResult<U> other) {
        if (other.is_ok()) {
            this->set_value(move(other.value()));
        } else {
            this->set_error(move(other.error()));
        }
    }
};

struct ExprBlock {
    ast::ExprList<> block;
    Span span;
};

class Parser {
public:
    using AttributeFunc = ErrorOr<Attribute>(*)(Parser &);
    
    Parser(Vector<Token> tokens);

    void set_attributes(HashMap<StringView, AttributeFunc> attributes);

    Token next();
    void skip(size_t n = 1);

    Token rewind(size_t n = 1);

    Token const& peek(size_t offset = 1) const;
    Token& peek(size_t offset = 1);

    [[nodiscard]] ErrorOr<Token> expect(TokenKind, StringView value = {});
    [[nodiscard]] Optional<Token> try_expect(TokenKind);

    [[nodiscard]] ErrorOr<void> expect_and(TokenKind kind) {
        TRY(this->expect(kind));
        return {};
    }

    template<typename... Args> requires(of_type_v<TokenKind, Args...> && sizeof...(Args) > 0)
    [[nodiscard]] ErrorOr<void> expect_and(TokenKind kind, Args... args) {
        auto result = this->expect(kind);
        if (result.is_err()) {
            return result.error();
        }

        return this->expect_and(args...);
    }

    bool is_valid_attribute(llvm::StringRef name);

    AttributeHandler::Result handle_expr_attributes(const ast::Attributes& attrs);

    bool is_upcoming_constructor(const ast::Expr& previous) const;

    ErrorOr<ExprBlock> parse_expr_block();
    
    ParseResult<ast::TypeExpr> parse_type();

    ParseResult<ast::BlockExpr> parse_block();

    ErrorOr<Vector<ast::Parameter>> parse_function_parameters();
    ParseResult<ast::FunctionDeclExpr> parse_function_decl(ast::LinkageSpecifier linkage, bool with_name = true);
    ParseResult<ast::Expr> parse_function(ast::LinkageSpecifier linkage = ast::LinkageSpecifier::None);

    ErrorOr<Vector<ast::GenericParameter>> parse_generic_parameters();
    ErrorOr<ast::ExprList<ast::TypeExpr>> parse_generic_arguments();

    ErrorOr<ast::Ident> parse_identifier();

    ParseResult<ast::IfExpr> parse_if();
    ParseResult<ast::Expr> parse_for();

    ParseResult<ast::StructExpr> parse_struct();

    ParseResult<ast::Expr> parse_single_variable_definition(bool is_const = false); // let foo = ...;
    ParseResult<ast::Expr> parse_tuple_variable_definition(); // let (foo, bar) = ...;
    ParseResult<ast::Expr> parse_variable_definition(bool allow_tuple = false, bool is_const = false);

    ParseResult<ast::Expr> parse_extern(ast::LinkageSpecifier linkage);
    ParseResult<ast::Expr> parse_extern_block();

    ParseResult<ast::EnumExpr> parse_enum();

    ParseResult<ast::TypeAliasExpr> parse_type_alias();

    ParseResult<ast::Expr> parse_anonymous_function();

    ParseResult<ast::MatchExpr> parse_match();

    ParseResult<ast::ImplExpr> parse_impl();

    ParseResult<ast::CallExpr> parse_call(OwnPtr<ast::Expr> callee);

    // Parses `foo::bar::baz` into a deque of strings
    [[nodiscard]] ErrorOr<Path> parse_path(Optional<String> name = {});

    ParseResult<ast::Expr> parse_immediate_binary_op(OwnPtr<ast::Expr> right, OwnPtr<ast::Expr> left, TokenKind op);
    ParseResult<ast::Expr> parse_immediate_unary_op(OwnPtr<ast::Expr> expr, TokenKind op);

    ErrorOr<ast::Attributes> parse_attributes();

    ErrorOr<ast::ExprList<>> parse();
    ErrorOr<ast::ExprList<>> statements();

    ParseResult<ast::Expr> statement();
    ParseResult<ast::Expr> expr(bool enforce_semicolon = true);
    ParseResult<ast::Expr> binary(i32 prec, OwnPtr<ast::Expr> left);
    ParseResult<ast::Expr> unary();
    ParseResult<ast::Expr> call();
    ParseResult<ast::Expr> attribute(OwnPtr<ast::Expr> expr);
    ParseResult<ast::Expr> index(OwnPtr<ast::Expr> expr);
    ParseResult<ast::Expr> primary();

private:
    Vector<Token> m_tokens;

    size_t m_offset = 0;
    Token m_current;

    bool m_in_function = false;
    bool m_in_loop = false;
    bool m_in_struct = false;
    bool m_self_allowed = false;

    HashMap<StringView, AttributeFunc> m_attributes;
};

}