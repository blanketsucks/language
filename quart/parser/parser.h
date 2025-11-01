#pragma once

#include <quart/lexer/tokens.h>
#include <quart/parser/ast.h>

#include <map>

namespace quart {

template<typename T>
class ParseResult : public ErrorOr<OwnPtr<T>> {
public:
    ParseResult() = default;

    ParseResult(OwnPtr<T> value) : ErrorOr<OwnPtr<T>>(move(value)) {}
    ParseResult(Error error) : ErrorOr<OwnPtr<T>>(move(error)) {}

    template<typename U> requires(std::is_base_of_v<T, U>)
    ParseResult(ParseResult<U> other) {
        if (other.is_ok()) {
            this->set_value(move(other.release_value()));
        } else {
            this->set_error(move(other.release_error()));
        }
    }
};

struct ExprBlock {
    ast::ExprList<> block;
    Span span;
};

class Parser {
public:
    enum State {
        None        = 0,
        InFunction  = 1 << 0,
        InLoop      = 1 << 1,
        InStruct    = 1 << 2,
        SelfAllowed = 1 << 3,
        Public      = 1 << 4
    };

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

    ErrorOr<ast::FunctionParameters> parse_function_parameters();
    ParseResult<ast::FunctionDeclExpr> parse_function_decl(
        LinkageSpecifier linkage,
        bool with_name = true,
        bool is_public = false,
        bool is_async = false
    );
    ParseResult<ast::Expr> parse_function(
        LinkageSpecifier linkage = LinkageSpecifier::None, bool is_public = false, bool is_async = false
    );

    ErrorOr<Vector<ast::GenericParameter>> parse_generic_parameters();
    ErrorOr<ast::ExprList<ast::TypeExpr>> parse_generic_arguments();

    ErrorOr<ast::Ident> parse_identifier();

    ParseResult<ast::IfExpr> parse_if();
    ParseResult<ast::Expr> parse_for();

    ParseResult<ast::StructExpr> parse_struct(bool is_public = false);

    ParseResult<ast::Expr> parse_single_variable_definition(bool is_const = false, bool is_public = false); // let foo = ...;
    ParseResult<ast::Expr> parse_tuple_variable_definition(); // let (foo, bar) = ...;
    ParseResult<ast::Expr> parse_variable_definition(bool allow_tuple = false, bool is_const = false, bool is_public = false);

    ParseResult<ast::Expr> parse_extern(LinkageSpecifier linkage, bool is_public = false);
    ParseResult<ast::Expr> parse_extern_block(bool is_public = false);

    ParseResult<ast::EnumExpr> parse_enum();

    ParseResult<ast::TypeAliasExpr> parse_type_alias(bool is_public = false);

    ParseResult<ast::Expr> parse_anonymous_function();

    ParseResult<ast::MatchExpr> parse_match();

    ParseResult<ast::Expr> parse_impl();

    ParseResult<ast::TraitExpr> parse_trait();

    ParseResult<ast::Expr> parse_pub();
    ParseResult<ast::Expr> parse_async();

    ParseResult<ast::CallExpr> parse_call(OwnPtr<ast::Expr> callee);

    // Parses `foo::bar::baz` into a deque of strings
    [[nodiscard]] ErrorOr<Path> parse_path(Optional<String> name = {}, ast::ExprList<ast::TypeExpr> arguments = {}, bool ignore_last = false);

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

static const std::map<StringView, ast::BuiltinType> STR_TO_TYPE = {
    { "void", ast::BuiltinType::Void },
    { "bool", ast::BuiltinType::Bool },
    { "i8", ast::BuiltinType::i8 },
    { "i16", ast::BuiltinType::i16 },
    { "i32", ast::BuiltinType::i32 },
    { "i64", ast::BuiltinType::i64 },
    { "i128", ast::BuiltinType::i128 },
    { "u8", ast::BuiltinType::u8 },
    { "u16", ast::BuiltinType::u16 },
    { "u32", ast::BuiltinType::u32 },
    { "u64", ast::BuiltinType::u64 },
    { "u128", ast::BuiltinType::u128 },
    { "f32", ast::BuiltinType::f32 },
    { "f64", ast::BuiltinType::f64 },
    { "isize", ast::BuiltinType::isize },
    { "usize", ast::BuiltinType::usize }
};

}