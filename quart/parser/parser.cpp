#include <algorithm>
#include <quart/parser/parser.h>
#include <quart/attributes/attributes.h>
#include <quart/parser/ast.h>
#include <quart/language/structs.h>

#include <llvm/ADT/StringRef.h>

#include <memory>
#include <cstring>

#define BOOL_EXPR(name) {                                   \
    Span span = m_current.span();                           \
    this->next();                                           \
    expr = make<ast::BoolExpr>(span, ast::BoolExpr::name);  \
    break; }

namespace quart {

constexpr int DECIMAL_BASE = 10;
constexpr int HEX_BASE = 16;
constexpr int BINARY_BASE = 2;

String Path::format() const {
    String fmt = {};
    if (m_segments.empty()) {
        return name();
    }

    for (auto& segment : m_segments) {
        fmt.append(segment.name());
        fmt.append("::");
    }

    fmt.append(name());
    return fmt;
}

void Path::rearrange() {
    if (m_segments.empty()) {
        return;
    }

    PathSegment segment = move(m_last);
    m_last = move(m_segments.back());

    m_segments.pop_back();
    m_segments.push_back(move(segment));
}

static const HashMap<StringView, ast::IntegerSuffix> INTEGER_SUFFIXES = {
    { "i8", { ast::BuiltinType::i8 } },
    { "i16", { ast::BuiltinType::i16 } },
    { "i32", { ast::BuiltinType::i32 } },
    { "i64", { ast::BuiltinType::i64} },
    { "i128", { ast::BuiltinType::i128 } },
    { "usize", { ast::BuiltinType::usize } },
    { "isize", { ast::BuiltinType::isize } }
};

Parser::Parser(Vector<Token> tokens) : m_tokens(move(tokens)), m_current(m_tokens.front()) {
    Attributes::init(*this);
}

void Parser::set_attributes(HashMap<StringView, AttributeFunc> attributes) {
    m_attributes = move(attributes);
}

Token Parser::next() {
    m_offset++;

    if (m_offset >= m_tokens.size()) {
        m_current = m_tokens.back(); // EOF
    } else {
        m_current = m_tokens[m_offset];
    }

    return m_current;
}

void Parser::skip(size_t n) {
    for (size_t i = 0; i < n; i++) {
        this->next();
    }
}

Token const& Parser::peek(size_t offset) const {
    if (m_offset >= m_tokens.size()) {
        return m_tokens.back(); // EOF
    }

    return m_tokens[m_offset + offset];
}

Token& Parser::peek(size_t offset) {
    if (m_offset >= m_tokens.size()) {
        return m_tokens.back(); // EOF
    }

    return m_tokens[m_offset + offset];
}

ErrorOr<Token> Parser::expect(TokenKind kind, StringView value) {
    auto span = m_current.span();
    auto option = this->try_expect(kind);

    if (option.has_value()) {
        return option.value();
    }

    if (value.empty()) {
        return err(span, "Expected '{}'", token_kind_to_str(kind));
    } else {
        return err(span, "Expected '{}'", value);
    }
}

Optional<Token> Parser::try_expect(TokenKind kind) {
    if (m_current.kind() != kind) {
        return {};
    }

    Token token = m_current;
    this->next();

    return token;
}

bool Parser::is_valid_attribute(llvm::StringRef name) {
    return m_attributes.find(name) != m_attributes.end();
}

bool Parser::is_upcoming_constructor(ast::Expr const& previous) const {
    if (m_current.kind() != TokenKind::LBrace) {
        return false;
    }

    auto& peek = this->peek();
    if (peek.kind() == TokenKind::RBrace) {
        return true;
    }
    
    if (!peek.is(TokenKind::Identifier) || !this->peek(2).is(TokenKind::Colon)) {
        return false;
    }

    return previous.is(ast::ExprKind::Identifier, ast::ExprKind::Path);
}

AttributeHandler::Result Parser::handle_expr_attributes(const ast::Attributes& attributes) {
    for (auto& [_, attribute] : attributes) {
        auto result = AttributeHandler::handle(*this, attribute);
        if (result != AttributeHandler::Ok) {
            return result;
        }
    }

    return AttributeHandler::Ok;
}

ParseResult<ast::TypeExpr> Parser::parse_type() {
    Span start = m_current.span();

    switch (m_current.kind()) {
        case TokenKind::And: {
            this->next();
            bool is_mutable = this->try_expect(TokenKind::Mut).has_value();

            return { make<ast::ReferenceTypeExpr>(m_current.span(), TRY(this->parse_type()), is_mutable) };
        }
        case TokenKind::Mul: {
            this->next();
            bool is_mutable = this->try_expect(TokenKind::Mut).has_value();

            auto type = TRY(this->parse_type());
            if (type->is<ast::ReferenceTypeExpr>()) {
                return err(type->span(), "Cannot create a pointer type to a reference");
            }

            return { make<ast::PointerTypeExpr>(m_current.span(), move(type), is_mutable) };
        }
        case TokenKind::LParen: {
            this->next();
            if (m_current.is(TokenKind::RParen)) {
                return err(m_current.span(), "Tuple types mist at least have a single element");
            }

            ast::ExprList<ast::TypeExpr> elements;
            while (!m_current.is(TokenKind::RParen)) {
                elements.push_back(TRY(this->parse_type()));
                if (!m_current.is(TokenKind::Comma)) {
                    break;
                }

                this->next();
            }

            Span end = TRY(this->expect(TokenKind::RParen)).span();
            Span span { start, end };

            return { make<ast::TupleTypeExpr>(span, move(elements)) };
        }
        case TokenKind::LBracket: {
            this->next();
            auto type = TRY(this->parse_type());

            TRY(this->expect(TokenKind::SemiColon));
            auto size = TRY(this->expr(false));

            Span end = TRY(this->expect(TokenKind::RBracket)).span();
            Span span { start, end };

            return { make<ast::ArrayTypeExpr>(span, move(type), move(size)) };
        }
        case TokenKind::Identifier: {
            String name = m_current.value();
            this->next();

            if (name == "int") {
                TRY(this->expect(TokenKind::LParen));
                auto size = TRY(this->expr(false));

                Span end = TRY(this->expect(TokenKind::RParen)).span();
                Span span { start, end };

                return { make<ast::IntegerTypeExpr>(span, move(size)) };
            }

            auto iterator = STR_TO_TYPE.find(name);
            if (iterator != STR_TO_TYPE.end()) {
                Span span { start, m_current.span() };
                return { make<ast::BuiltinTypeExpr>(span, iterator->second) };
            }

            Path path = TRY(this->parse_path(name));
            Span span { start, m_current.span() };

            auto type = make<ast::NamedTypeExpr>(span, move(path));
            if (m_current.is(TokenKind::Lt)) {
                auto args = TRY(this->parse_generic_arguments());
                span = { start, m_current.span() };

                return { make<ast::GenericTypeExpr>(span, move(type), move(args)) };
            }

            return { move(type) };
        }
        case TokenKind::Func: {
            this->next();
            TRY(this->expect(TokenKind::LParen));

            ast::ExprList<ast::TypeExpr> params;
            while (!m_current.is(TokenKind::RParen)) {
                params.push_back(TRY(this->parse_type()));
                if (!m_current.is(TokenKind::Comma)) {
                    break;
                }
                
                this->next();
            }

            Span end = TRY(this->expect(TokenKind::RParen)).span();
            OwnPtr<ast::TypeExpr> return_type = nullptr;

            if (m_current.is(TokenKind::Arrow)) {
                end = this->next().span();
                return_type = TRY(this->parse_type());
            }

            Span span { start, end };
            return { make<ast::FunctionTypeExpr>(span, move(params), move(return_type)) };
        }
        default:
            return err(m_current.span(), "Expected type");
    }
}
ErrorOr<ExprBlock> Parser::parse_expr_block() {
    ast::ExprList<> block;
    Span start = m_current.span();

    while (!m_current.is(TokenKind::RBrace)) {
        ast::Attributes attrs = TRY(this->parse_attributes());
        auto expr = TRY(this->statement());

        if (expr) {
            if (this->handle_expr_attributes(attrs) != AttributeHandler::Ok) {
                continue;
            }

            expr->attributes().insert(attrs);
            block.push_back(move(expr));
        }
    }

    Span end = TRY(this->expect(TokenKind::RBrace)).span();
    Span span { start, end };

    return ExprBlock { move(block), span };
}

ParseResult<ast::BlockExpr> Parser::parse_block() {
    auto [block, span] = TRY(this->parse_expr_block());
    return { make<ast::BlockExpr>(span, move(block)) };
}

ErrorOr<Vector<ast::GenericParameter>> Parser::parse_generic_parameters() {
    Vector<ast::GenericParameter> parameters;

    TRY(this->expect(TokenKind::Lt));
    while (!m_current.is(TokenKind::Gt)) {
        Token token = TRY(this->expect(TokenKind::Identifier));

        String name = token.value();
        Span span = token.span();

        ast::ExprList<ast::TypeExpr> constraints;
        OwnPtr<ast::TypeExpr> default_type = nullptr;

        if (m_current.is(TokenKind::Colon)) {
            this->next();

            auto should_continue_parsing = [](Token& token) {
                return !token.is(TokenKind::Comma) && !token.is(TokenKind::Gt) && !token.is(TokenKind::Assign);
            };

            while (should_continue_parsing(m_current)) {
                constraints.push_back(TRY(this->parse_type()));
                if (!m_current.is(TokenKind::Add)) {
                    break;
                }

                this->next();
            }
        }

        if (m_current.is(TokenKind::Assign)) {
            this->next();
            default_type = TRY(this->parse_type());
        }

        parameters.push_back({
            move(name),
            move(constraints), 
            move(default_type), 
            span
        });

        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        this->next();
    }

    TRY(this->expect(TokenKind::Gt));
    return parameters;
}

ErrorOr<ast::ExprList<ast::TypeExpr>> Parser::parse_generic_arguments() {
    ast::ExprList<ast::TypeExpr> arguments;

    TRY(this->expect(TokenKind::Lt));
    while (!m_current.is(TokenKind::Gt)) {
        arguments.push_back(TRY(this->parse_type()));
        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        this->next();
    }

    TRY(this->expect(TokenKind::Gt));
    return arguments;
}

ParseResult<ast::TypeAliasExpr> Parser::parse_type_alias(bool is_public) {
    Span span = m_current.span();
    Vector<ast::GenericParameter> parameters;

    String name = TRY(this->expect(TokenKind::Identifier)).value();
    if (m_current.is(TokenKind::Lt)) {
        parameters = TRY(this->parse_generic_parameters());
    }

    TRY(this->expect(TokenKind::Assign));

    auto type = TRY(this->parse_type());
    TRY(this->expect(TokenKind::SemiColon));

    return { make<ast::TypeAliasExpr>(span, move(name), move(type), move(parameters), is_public) };
}

ErrorOr<ast::FunctionParameters> Parser::parse_function_parameters() {
    Vector<ast::Parameter> params;

    bool has_kwargs = false;
    bool has_default_values = false;
    bool is_c_variadic = false;
    bool is_variadic = false;

    while (!m_current.is(TokenKind::RParen)) {
        if (!m_current.is(TokenKind::Mut, TokenKind::Identifier, TokenKind::Mul, TokenKind::Ellipsis)) {
            return err(m_current.span(), "Expected parameter name, 'mut', '*', or '...'");
        }

        u8 flags = 0;
        Span span = m_current.span();

        String name;
        switch (m_current.kind()) {
            case TokenKind::Ellipsis: {
                is_c_variadic = true;
                this->next();

                goto end;
            } break;
            case TokenKind::Mut: {
                this->next();
                flags |= FunctionParameter::Mutable;

                Token token = TRY(this->expect(TokenKind::Identifier));

                name = token.value();
                span = Span { span, token.span() };
            } break;
            case TokenKind::Identifier: {
                name = m_current.value();
                this->next();
            } break;
            default:
                ASSERT(false, "Unreachable");
        }

        OwnPtr<ast::TypeExpr> type = nullptr;
        
        // FIXME: A function only has one `self` parameter
        if (name == "self" && m_self_allowed) {
            flags |= FunctionParameter::Self;
        } else {
            TRY(this->expect(TokenKind::Colon));
            type = TRY(this->parse_type());
        }

        OwnPtr<ast::Expr> default_value = nullptr;
        if (m_current.is(TokenKind::Assign)) {
            has_default_values = true;
            this->next();

            default_value = TRY(this->expr(false));
        } else {
            if (has_default_values) {
                return err(m_current.span(), "Every parameter following one with a default value must also have a default value");
            }
        }

        params.push_back({ move(name), move(type), move(default_value), flags, span });
        if (is_c_variadic) {
            break;
        }

        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        this->next();
    }

end:
    TRY(this->expect(TokenKind::RParen));
    return ast::FunctionParameters { move(params), is_c_variadic };
}

ParseResult<ast::FunctionDeclExpr> Parser::parse_function_decl(LinkageSpecifier linkage, bool with_name, bool is_public) {
    Span start = m_current.span();
    String name = "<anonymous>";

    Span span;
    if (with_name) {
        Token token = TRY(this->expect(TokenKind::Identifier, "function name"));
        
        name = token.value();
        span = token.span();
        
        TRY(this->expect(TokenKind::LParen));
    }

    auto [params, is_c_variadic] = TRY(this->parse_function_parameters());

    Span end = m_current.span();
    OwnPtr<ast::TypeExpr> return_type = nullptr;

    if (m_current.is(TokenKind::Arrow)) {
        end = this->next().span();
        return_type = TRY(this->parse_type());
    }

    if (!span.start()) {
        span = { start, end };
    }
    
    return { make<ast::FunctionDeclExpr>(span, move(name), move(params), move(return_type), linkage, is_c_variadic, is_public) };
}

ParseResult<ast::Expr> Parser::parse_function(LinkageSpecifier linkage, bool is_public) {
    auto decl = TRY(this->parse_function_decl(linkage, true, is_public));
    if (m_current.is(TokenKind::SemiColon)) {
        this->next();
        return { move(decl) };
    }
    
    TRY(this->expect(TokenKind::LBrace));
    m_in_function = true;

    auto [body, _] = TRY(this->parse_expr_block());
    m_in_function = false;

    return { make<ast::FunctionExpr>(decl->span(), move(decl), move(body)) };
}

ParseResult<ast::IfExpr> Parser::parse_if() {
    Span start = m_current.span();
    auto condition = TRY(this->expr(false));

    OwnPtr<ast::Expr> body;
    if (!m_current.is(TokenKind::LBrace)) {
        body = TRY(this->statement());
    } else {
        this->next();
        body = TRY(this->parse_block());
    }

    Span end = body->span();
    OwnPtr<ast::Expr> else_body = nullptr;
    if (m_current.is(TokenKind::Else)) {
        this->next();
        if (!m_current.is(TokenKind::LBrace)) {
            else_body = TRY(this->statement());
        } else {
            this->next();
            else_body = TRY(this->parse_block());
        }

        end = else_body->span();
    }

    Span span { start, end };
    return { make<ast::IfExpr>(span, move(condition), move(body), move(else_body)) };
}

ParseResult<ast::Expr> Parser::parse_for() {
    ast::Ident ident = TRY(this->parse_identifier());

    TRY(this->expect(TokenKind::In));

    bool has_outer_loop = m_in_loop;
    m_in_loop = true;

    auto expr = TRY(this->expr(false));
    if (m_current.is(TokenKind::DoubleDot)) {
        this->next();
        bool inclusive = false;

        if (m_current.is(TokenKind::Assign)) {
            this->next();
            inclusive = true;
        }

        OwnPtr<ast::Expr> end = nullptr;
        if (!m_current.is(TokenKind::LBrace) || inclusive) {
            end = TRY(this->expr(false));
        }
        
        TRY(this->expect(TokenKind::LBrace));

        auto body = TRY(this->parse_block());
        m_in_loop = has_outer_loop;

        return { make<ast::RangeForExpr>(ident.span, move(ident), inclusive, move(expr), move(end), move(body)) };
    }

    TRY(this->expect(TokenKind::LBrace));

    auto body = TRY(this->parse_block());
    m_in_loop = has_outer_loop;

    return { make<ast::ForExpr>(ident.span, move(ident), move(expr), move(body)) };
}

ParseResult<ast::StructExpr> Parser::parse_struct(bool is_public) {
    Span start = m_current.span();
    Token token = TRY(this->expect(TokenKind::Identifier, "struct name"));
    
    String name = token.value();
    Span end = token.span();

    Vector<ast::GenericParameter> parameters;
    Vector<ast::StructField> fields;
    ast::ExprList<> members;

    if (m_current.is(TokenKind::SemiColon)) {
        this->next();
        Span span { start, m_current.span() };

        return { make<ast::StructExpr>(span, move(name), true, move(parameters), move(fields), move(members), is_public) };
    }

    if (m_current.is(TokenKind::Lt)) {
        parameters = TRY(this->parse_generic_parameters());
    }

    m_in_struct = true;
    m_self_allowed = true;

    TRY(this->expect(TokenKind::LBrace));
    u32 index = 0;

    while (!m_current.is(TokenKind::RBrace)) {
        ast::Attributes attrs = TRY(this->parse_attributes());

        u8 flags = StructField::None;
        switch (m_current.kind()) {
            case TokenKind::Pub: {
                flags |= StructField::Public;
                this->next();
            } break;
            case TokenKind::Readonly: {
                flags |= StructField::Readonly;
                this->next();
            }
            default: break;
        }

        bool is_field_public = flags & StructField::Public;
        switch (m_current.kind()) {
            case TokenKind::Func: {
                this->next();
                auto expr = TRY(this->parse_function(LinkageSpecifier::None, is_field_public));

                expr->attributes().insert(attrs);
                members.push_back(move(expr));
            } break;
            case TokenKind::Const: {
                this->next();
                members.push_back(TRY(this->parse_variable_definition(false, true, is_field_public)));
            } break;
            case TokenKind::Type: {
                this->next();
                members.push_back(TRY(this->parse_type_alias(is_field_public)));
            } break;
            case TokenKind::Identifier: {
                String name = m_current.value();
                this->next();

                TRY(this->expect(TokenKind::Colon));
                fields.push_back({
                    move(name), 
                    TRY(this->parse_type()), 
                    index,
                    flags
                });

                TRY(this->expect(TokenKind::SemiColon));
                index++;
            } break;
            default:
                return err(m_current.span(), "Expected function definition, constant, type alias, or field");
        }
    }

    TRY(this->expect(TokenKind::RBrace));

    m_self_allowed = false;
    m_in_struct = false;

    Span span { start, end };
    return { make<ast::StructExpr>(span, move(name), false, move(parameters), move(fields), move(members), is_public) };   
}

ErrorOr<ast::Ident> Parser::parse_identifier() {
    bool is_mutable = this->try_expect(TokenKind::Mut).has_value();
    Token token = TRY(this->expect(TokenKind::Identifier, "identifier"));

    return ast::Ident { token.value(), is_mutable, token.span() };
}

ParseResult<ast::Expr> Parser::parse_single_variable_definition(bool is_const, bool is_public) {
    Span span = m_current.span();

    OwnPtr<ast::TypeExpr> type = nullptr;
    ast::Ident ident = TRY(this->parse_identifier());

    if (is_const && ident.is_mutable) {
        return err(m_current.span(), "Constants cannot be mutable");
    }

    if (m_current.is(TokenKind::Colon)) {
        this->next();
        type = TRY(this->parse_type());
    }

    OwnPtr<ast::Expr> expr = nullptr;
    Span end = {};

    if (!m_current.is(TokenKind::Assign)) {
        if (!type) {
            return err(m_current.span(), "Un-initialized variables must have a specified type");
        }

        end = TRY(this->expect(TokenKind::SemiColon)).span();
    } else {
        this->next();
        expr = TRY(this->expr(false));
        
        TRY(this->expect(TokenKind::SemiColon));
        end = expr->span();
    }

    if (is_const) {
        if (!expr) {
            return err(m_current.span(), "Constants must have an initializer");
        }

        return { make<ast::ConstExpr>(span, move(ident.value), move(type), move(expr), is_public) };
    }

    return { make<ast::AssignmentExpr>(span, move(ident), move(type), move(expr), is_public) };
}

ParseResult<ast::Expr> Parser::parse_tuple_variable_definition() {
    Span span = m_current.span();

    OwnPtr<ast::TypeExpr> type = nullptr;
    Vector<ast::Ident> idents;

    bool all_mutable = this->try_expect(TokenKind::Mut).has_value();

    TRY(this->expect(TokenKind::LParen));
    while (!m_current.is(TokenKind::RParen)) {
        // if (all_mutable && m_current == TokenKind::Mut) {
        //     this->next();
        //     NOTE(m_current.span, "Redundant 'mut'");
        // }

        ast::Ident ident = TRY(this->parse_identifier());
        ident.is_mutable = all_mutable || ident.is_mutable;
        
        idents.push_back(move(ident));
        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        this->next();
    }

    if (m_current.is(TokenKind::Colon)) {
        this->next();
        type = TRY(this->parse_type());
    }

    OwnPtr<ast::Expr> expr = nullptr;
    Span end = {};

    if (!m_current.is(TokenKind::Assign)) {
        if (!type) {
            return err(m_current.span(), "Un-initialized variables must have a specified type");
        }

        end = TRY(this->expect(TokenKind::SemiColon)).span();
    } else {
        this->next();
        expr = TRY(this->expr(false));
        
        TRY(this->expect(TokenKind::SemiColon));
        end = expr->span();
    }

    return { make<ast::TupleAssignmentExpr>(span, move(idents), move(type), move(expr)) };
}

ParseResult<ast::Expr> Parser::parse_variable_definition(bool allow_tuple, bool is_const, bool is_public) {
    if (allow_tuple && m_current.is(TokenKind::LParen)) {
        return this->parse_tuple_variable_definition();
    }

    return this->parse_single_variable_definition(is_const, is_public);
}

ParseResult<ast::Expr> Parser::parse_extern(LinkageSpecifier linkage, bool is_public) {
    OwnPtr<ast::Expr> definition;
    
    ast::Attributes attrs = TRY(this->parse_attributes());

    if (m_current.is(TokenKind::Pub)) {
        if (is_public) {
            note(m_current.span(), "Redundant pub");
        }

        is_public = true;
        this->next();
    }

    if (m_current.is(TokenKind::Let)) {
        this->next();
        definition = TRY(this->parse_single_variable_definition(false, is_public));
    } else {
        if (!m_current.is(TokenKind::Func)) {
            return err(m_current.span(), "Expected function definition or variable declaration");
        }

        this->next();
        definition = TRY(this->parse_function(linkage, is_public));
    }

    definition->attributes().insert(attrs);
    return definition;
}

ParseResult<ast::Expr> Parser::parse_extern_block(bool is_public) {
    Span start = m_current.span();
    LinkageSpecifier linkage = LinkageSpecifier::Unspecified;

    if (m_current.is(TokenKind::String)) {
        if (m_current.value() != "C") {
            return err(m_current.span(), "Unknown linkage specifier '{}'", m_current.value());
        }

        this->next();
        linkage = LinkageSpecifier::C;
    }

    if (m_current.is(TokenKind::LBrace)) {
        ast::ExprList<> definitions;
        this->next();

        while (!m_current.is(TokenKind::RBrace)) {
            auto expr = TRY(this->parse_extern(linkage, is_public));
            if (this->handle_expr_attributes(expr->attributes()) != AttributeHandler::Ok) {
                continue;
            }

            definitions.push_back(move(expr));
        }

        Span end = TRY(this->expect(TokenKind::RBrace)).span();
        Span span = { start, end };

        return { make<ast::ExternBlockExpr>(span, move(definitions)) };
    }

    return this->parse_extern(linkage, is_public);
}

ParseResult<ast::EnumExpr> Parser::parse_enum() {
    Span start = m_current.span();
    String name = TRY(this->expect(TokenKind::Identifier, "enum name")).value();

    OwnPtr<ast::TypeExpr> type = nullptr;
    if (m_current.is(TokenKind::Colon)) {
        this->next(); 
        type = TRY(this->parse_type());
    }

    TRY(this->expect(TokenKind::LBrace));

    Vector<ast::EnumField> fields;
    while (!m_current.is(TokenKind::RBrace)) {
        String field = TRY(this->expect(TokenKind::Identifier, "enum field name")).value();
        OwnPtr<ast::Expr> value = nullptr;

        if (m_current.is(TokenKind::Assign)) {
            this->next();
            value = TRY(this->expr(false));
        }

        fields.push_back({ move(field), move(value) });
        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        this->next();
    }

    Span end = TRY(this->expect(TokenKind::RBrace)).span();
    Span span { start, end };

    return { make<ast::EnumExpr>(span, move(name), move(type), move(fields)) };
}

ParseResult<ast::Expr> Parser::parse_anonymous_function() {
    auto [params, _] = TRY(this->parse_function_parameters());
    OwnPtr<ast::TypeExpr> return_type = nullptr;

    if (m_current.is(TokenKind::Colon)) {
        this->next();
        return_type = TRY(this->parse_type());
    }

    TRY(this->expect(TokenKind::FatArrow));

    ast::ExprList<> body;
    body.push_back(TRY(this->expr(false)));

    Span start = params.front().span;
    Span end = params.back().span;

    Span span = { start, end };
    auto decl = make<ast::FunctionDeclExpr>(
        span, "<anonymous>", move(params), move(return_type), LinkageSpecifier::None, false, true
    );

    return { make<ast::FunctionExpr>(span, move(decl), move(body)) };
}

ParseResult<ast::CallExpr> Parser::parse_call(OwnPtr<ast::Expr> callee) {
    ast::ExprList<> args;
    HashMap<String, OwnPtr<ast::Expr>> kwargs;

    bool has_kwargs = false;
    while (!m_current.is(TokenKind::RParen)) {
        if (m_current.is(TokenKind::Identifier) && this->peek().is(TokenKind::Colon)) {
            String name = m_current.value();
            this->skip(2);

            auto expr = TRY(this->expr(false));
            kwargs[name] = move(expr);
        } else {
            if (has_kwargs) {
                return err(m_current.span(), "Positional arguments must come before keyword arguments");
            }

            auto expr = TRY(this->expr(false));
            args.push_back(move(expr));
        }

        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        this->next();
    }

    TRY(this->expect(TokenKind::RParen));
    Span span = { callee->span(), m_current.span() };

    return { make<ast::CallExpr>(span, move(callee), move(args), move(kwargs)) };
}

ParseResult<ast::MatchExpr> Parser::parse_match() {
    auto value = TRY(this->expr(false));
    TRY(this->expect(TokenKind::LBrace));

    Vector<ast::MatchArm> arms;
    bool has_wildcard = false;

    size_t index = 0;
    while (!m_current.is(TokenKind::RBrace)) {
        ast::MatchPattern pattern;

        if (m_current.is(TokenKind::Else)) {
            if (has_wildcard) {
                return err(m_current.span(), "Wildcard pattern already exists");
            }

            pattern.is_wildcard = true;
            pattern.span = m_current.span();

            has_wildcard = true;

            this->next();
        } else if (m_current.is(TokenKind::If)) {
            this->next();

            auto expr = TRY(this->expr(false));
            Span span = expr->span();

            pattern.values.push_back(move(expr));
            pattern.is_conditional = true;

            pattern.span = span;
        } else {
            auto expr = TRY(this->expr(false));
            Span span = expr->span();

            pattern.values.push_back(move(expr));
            while (!m_current.is(TokenKind::FatArrow)) {
                expr = TRY(this->expr(false));
                span = { span, expr->span() };

                pattern.values.push_back(move(expr));
                if (!m_current.is(TokenKind::Or)) {
                    break;
                }

                this->next();
            }

            pattern.span = span;
        }

        TRY(this->expect(TokenKind::FatArrow));
        auto body = TRY(this->expr(false));

        arms.push_back({ move(pattern), move(body), index });
        if (!m_current.is(TokenKind::Comma)) {
            break;
        }

        index++;
        this->next();
    }

    TRY(this->expect(TokenKind::RBrace));
    return { make<ast::MatchExpr>(value->span(), move(value), move(arms)) };
}

ParseResult<ast::Expr> Parser::parse_impl() {
    Vector<ast::GenericParameter> parameters;
    if (m_current.is(TokenKind::Lt)) {
        parameters = TRY(this->parse_generic_parameters());
    }

    auto type = TRY(this->parse_type());
    OwnPtr<ast::TypeExpr> trait = nullptr;

    if (m_current.is(TokenKind::For)) {
        trait = move(type);
        this->next();

        type = TRY(this->parse_type());
    }

    TRY(this->expect(TokenKind::LBrace));

    ast::ExprList<> body;
    m_self_allowed = true;

    while (!m_current.is(TokenKind::RBrace)) {
        // FIXME: Handle attributes
        // FIXME: Should we allow definitions other than functions?
        if (!m_current.is(TokenKind::Func)) {
            return err(m_current.span(), "Expected function definition");
        }

        switch (m_current.kind()) {
            case TokenKind::Func:
                this->next();
                body.push_back(TRY(this->parse_function()));
                
                break;
            default:
                ASSERT(false, "Unreachable");
        }
    }

    TRY(this->expect(TokenKind::RBrace));
    Span span = type->span();

    if (trait) {
        return { make<ast::ImplTraitExpr>(span, move(trait), move(type), move(body)) };
    }

    auto block = make<ast::BlockExpr>(span, move(body));
    return { make<ast::ImplExpr>(span, move(type), move(block), move(parameters)) };
}

ParseResult<ast::TraitExpr> Parser::parse_trait() {
    Token token = TRY(this->expect(TokenKind::Identifier));

    String name = token.value();
    Span span = token.span();

    ast::ExprList<> body;
    m_self_allowed = true;

    TRY(this->expect(TokenKind::LBrace));
    while (!m_current.is(TokenKind::RBrace)) {
        if (!m_current.is(TokenKind::Func)) {
            return err(m_current.span(), "Expected function definition");
        }

        switch (m_current.kind()) {
            case TokenKind::Func:
                this->next();

                body.push_back(TRY(this->parse_function()));
                break;
            default:
                ASSERT(false, "Unreachable");
        }
    }

    TRY(this->expect(TokenKind::RBrace));
    return { make<ast::TraitExpr>(span, move(name), move(body)) };
}
 
ParseResult<ast::Expr> Parser::parse_pub() {
    switch (m_current.kind()) {
        case TokenKind::Func:
            this->next();
            return this->parse_function(LinkageSpecifier::None, true);
        case TokenKind::Struct:
            this->next();
            return this->parse_struct(true);
        case TokenKind::Type:
            this->next();
            return this->parse_type_alias(true);
        case TokenKind::Enum:
            this->next();
            return this->parse_enum();
        case TokenKind::Trait:
            this->next();
            return this->parse_trait();
        case TokenKind::Impl:
            this->next();
            return this->parse_impl();
        case TokenKind::Extern:
            this->next();
            return this->parse_extern_block(true);
        case TokenKind::Const:
            this->next();
            return this->parse_variable_definition(false, true, true);
        case TokenKind::Let:
            this->next();
            return this->parse_variable_definition(false, false, true);
        default:
            return err(m_current.span(), "Invalid token for 'pub' keyword");
    }
}

ErrorOr<Path> Parser::parse_path(Optional<String> name, ast::ExprList<ast::TypeExpr> arguments, bool ignore_last) {
    PathSegment segment = {};
    if (!name.has_value()) {
        String name = TRY(this->expect(TokenKind::Identifier)).value();
        ast::ExprList<ast::TypeExpr> arguments;

        if (m_current.is(TokenKind::Lt)) {
            arguments = TRY(this->parse_generic_arguments());
        }

        segment = { move(name), move(arguments) };
    } else {
        ast::ExprList<ast::TypeExpr> args;
        if (arguments.empty() && m_current.is(TokenKind::Lt)) {
            args = TRY(this->parse_generic_arguments());
        } else {
            args = move(arguments);
        }

        segment = { move(name.value()), move(args) };
    }

    Path path { move(segment), {} };
    while (m_current.is(TokenKind::DoubleColon)) {
        this->next();

        auto option = this->try_expect(TokenKind::Identifier);
        if (!option.has_value() && ignore_last) {
            break;
        } else if (!option.has_value()) {
            return err(m_current.span(), "Expected 'identifier'");
        }

        ast::ExprList<ast::TypeExpr> arguments;
        if (m_current.is(TokenKind::Lt)) {
            arguments = TRY(this->parse_generic_arguments());
        }

        path.push({ option->value(), move(arguments) });
    }
    
    path.rearrange();
    return path;
}

ErrorOr<ast::ExprList<>> Parser::parse() {
    return this->statements();
}

ErrorOr<ast::ExprList<>> Parser::statements() {
    ast::ExprList<> statements;

    while (!m_current.is(TokenKind::EOS)) {
        ast::Attributes attrs = TRY(this->parse_attributes());
        auto expr = TRY(this->statement());

        if (expr) {
            if (this->handle_expr_attributes(attrs) != AttributeHandler::Ok) {
                continue;
            }

            expr->attributes().insert(attrs);
            statements.push_back(move(expr));
        }
    }

    return statements;
}

ParseResult<ast::Expr> Parser::statement() {
    switch (m_current.kind()) {
        case TokenKind::Extern: {
            this->next();
            return this->parse_extern_block();
        } 
        case TokenKind::Func: {
            this->next();
            return this->parse_function();
        }
        case TokenKind::Return: {
            if (!m_in_function) {
                return err(m_current.span(), "Return statement outside of function");
            }

            Span start = m_current.span();
            this->next();

            OwnPtr<ast::Expr> expr = nullptr;
            if (!m_current.is(TokenKind::SemiColon)) {
                expr = TRY(this->expr(false));
            }

            Span end = TRY(this->expect(TokenKind::SemiColon)).span();
            Span span = { start, end };

            return { make<ast::ReturnExpr>(span, move(expr)) };
        } 
        case TokenKind::If: {
            this->next();
            return this->parse_if();
        } 
        case TokenKind::Let: {
            this->next();
            return this->parse_variable_definition(false);
        }
        case TokenKind::Const: {
            this->next();
            return this->parse_variable_definition(true, true);
        }
        case TokenKind::Struct: {
            this->next();
            return this->parse_struct();
        }
        case TokenKind::Type: {
            this->next();
            return this->parse_type_alias();
        }
        case TokenKind::While: {
            bool has_outer_loop = m_in_loop;
            Span start = m_current.span();

            this->next();
            auto condition = TRY(this->expr(false));

            TRY(this->expect(TokenKind::LBrace));

            m_in_loop = true;
            auto body = TRY(this->parse_block());

            m_in_loop = has_outer_loop;
            Span span = { start, condition->span() };

            return { make<ast::WhileExpr>(span, move(condition), move(body)) };
        }
        case TokenKind::Break: {
            if (!m_in_loop) {
                return err(m_current.span(), "Break statement outside of loop");
            }

            Span span = m_current.span();
            this->next();

            TRY(this->expect(TokenKind::SemiColon));
            return { make<ast::BreakExpr>(span) };
        } 
        case TokenKind::Continue: {
            if (!m_in_loop) {
                return err(m_current.span(), "Continue statement outside of loop");
            }

            Span span = m_current.span();
            this->next();

            TRY(this->expect(TokenKind::SemiColon));
            return { make<ast::ContinueExpr>(span) };
        } 
        case TokenKind::Using: {
            Span start = m_current.span();
            this->next();

            Vector<String> symbols;

            if (!m_current.is(TokenKind::LParen)) {
                String symbol = TRY(this->expect(TokenKind::Identifier)).value();
                symbols.push_back(move(symbol));
            } else {
                this->next();
                while (!m_current.is(TokenKind::RParen)) {
                    String symbol = TRY(this->expect(TokenKind::Identifier)).value();
                    symbols.push_back(move(symbol));

                    if (!m_current.is(TokenKind::Comma)) {
                        break;
                    }
                    this->next();
                }

                TRY(this->expect(TokenKind::RParen));
            }

            Token token = TRY(this->expect(TokenKind::Identifier));
            if (token.value() != "from") {
                return err(token.span(), "Expected 'from'");
            }

            this->next();
            Path path = TRY(this->parse_path());

            Span span = start; // FIXME:
            return { make<ast::UsingExpr>(span, move(path), move(symbols)) };
        } 
        case TokenKind::Defer: {
            if (!m_in_function) {
                return err(m_current.span(), "Defer statement outside of function");
            }

            Span span = m_current.span();
            this->next();

            auto expr = TRY(this->expr());
            return { make<ast::DeferExpr>(span, move(expr)) };
        } 
        case TokenKind::Enum: {
            this->next();
            return this->parse_enum();
        } 
        case TokenKind::Import: {
            Span start = m_current.span();
            this->next();

            bool is_relative = false;
            if (m_current.is(TokenKind::DoubleColon)) {
                is_relative = true;
                this->next();
            }

            Path path = TRY(this->parse_path({}, {}, true));
            bool is_wildcard = false;

            Vector<String> symbols;
            if (m_current.is(TokenKind::Mul)) {
                is_wildcard = true;
                this->next();
            } else if (m_current.is(TokenKind::LBrace)) {
                this->next();
                while (!m_current.is(TokenKind::RBrace)) {
                    String symbol = TRY(this->expect(TokenKind::Identifier)).value();
                    symbols.push_back(move(symbol));

                    if (!m_current.is(TokenKind::Comma)) {
                        break;
                    }

                    this->next();
                }

                TRY(this->expect(TokenKind::RBrace));
            }

            Span end = TRY(this->expect(TokenKind::SemiColon)).span();
            Span span = { start, end };
            
            // FIXME: Rather than an ImportExpr, we should just resolve the everything here and return ModuleExpr
            return { make<ast::ImportExpr>(span, move(path), is_wildcard, is_relative, move(symbols)) };
        }
        case TokenKind::Module: {
            this->next();

            Token token = TRY(this->expect(TokenKind::Identifier));

            String name = token.value();
            Span span = token.span();

            TRY(this->expect(TokenKind::LBrace));
            auto [body, _] = TRY(this->parse_expr_block());

            return { make<ast::ModuleExpr>(span, move(name), move(body)) };
        }
        case TokenKind::For: {
            this->next();
            return this->parse_for();
        } 
        case TokenKind::StaticAssert: {
            Span span = m_current.span();
            this->next();

            TRY(this->expect(TokenKind::LParen));
            auto expr = TRY(this->expr(false));

            String message = {};
            if (m_current.is(TokenKind::Comma)) {
                this->next();
                message = TRY(this->expect(TokenKind::String)).value();
            }

            TRY(this->expect_and(TokenKind::RParen, TokenKind::SemiColon));
            return { make<ast::StaticAssertExpr>(span, move(expr), move(message)) };
        }
        case TokenKind::Impl: {
            this->next();
            return this->parse_impl();
        }
        case TokenKind::Trait: {
            this->next();
            return this->parse_trait();
        }
        case TokenKind::Pub: {
            this->next();
            return this->parse_pub();
        }
        case TokenKind::ConstEval: {
            this->next();
            TRY(this->expect(TokenKind::LBrace));

            auto [body, span] = TRY(this->parse_expr_block());
            return { make<ast::ConstEvalExpr>(span, move(body)) };
        };
        case TokenKind::SemiColon:
            this->next();
            return this->statement();
        default:
            return this->expr();
    }

    // Should be unreachable
    return err(m_current.span(), "Expected statement");
}

ErrorOr<ast::Attributes> Parser::parse_attributes() {
    ast::Attributes attrs;
    if (m_current.is(TokenKind::Not) && this->peek().is(TokenKind::LBracket)) {
        this->skip(2);

        while (!m_current.is(TokenKind::RBracket)) {
            Token token = TRY(this->expect(TokenKind::Identifier));
            if (!this->is_valid_attribute(token.value())) {
                return err(token.span(), "Unknown attribute '{}'", token.value());
            }

            auto& handler = m_attributes.at(token.value());
            attrs.insert(TRY(handler(*this)));

            if (m_current.is(TokenKind::Comma)) {
                this->next();
            }
        }
        
        TRY(this->expect(TokenKind::RBracket));
    }

    return attrs;
}

ParseResult<ast::Expr> Parser::expr(bool enforce_semicolon) {
    auto expr = TRY(this->binary(0, TRY(this->unary())));
    if (enforce_semicolon) {
        TRY(this->expect(TokenKind::SemiColon));
    }

    return expr;
}

ParseResult<ast::Expr> Parser::binary(i32 prec, OwnPtr<ast::Expr> left) {
    while (true) {
        i8 precedence = m_current.precedence();
        if (precedence < prec) {
            return left;
        }

        TokenKind kind = m_current.kind();
        BinaryOp op = BINARY_OPS.at(kind);

        this->next();
        auto right = TRY(this->unary());

        i8 next = m_current.precedence();
        if (precedence < next) {
            right = TRY(this->binary(precedence + 1, move(right)));
        }
       
        Span span = { left->span(), right->span() };
        if (INPLACE_OPERATORS.find(kind) != INPLACE_OPERATORS.end()) {
            left = make<ast::InplaceBinaryOpExpr>(span, op, move(left), move(right));
        } else {
            left = make<ast::BinaryOpExpr>(span, op, move(left), move(right));
        }
    }
}

ParseResult<ast::Expr> Parser::unary() {
    auto iterator = UNARY_OPS.find(m_current.kind());
    OwnPtr<ast::Expr> expr = nullptr;

    if (iterator == UNARY_OPS.end()) {
        expr = TRY(this->call());
    } else if (m_current.is(TokenKind::And)) {
        Span start = m_current.span();
        this->next();

        bool is_mutable = this->try_expect(TokenKind::Mut).has_value();

        auto value = TRY(this->call());

        Span span = Span { start, value->span() };
        expr = make<ast::ReferenceExpr>(span, move(value), is_mutable);
    } else {
        Span start = m_current.span();

        UnaryOp op = iterator->second;
        this->next();

        auto value = TRY(this->call());

        Span span = Span { start, value->span() };
        expr = make<ast::UnaryOpExpr>(span, move(value), op);
    }

    switch (m_current.kind()) {
        case TokenKind::As: {
            this->next();
            auto type = TRY(this->parse_type());

            Span span = { expr->span(), type->span() };
            return { make<ast::CastExpr>(span, move(expr), move(type)) };
        }
        case TokenKind::If: {
            this->next();
            auto condition = TRY(this->expr(false));

            TRY(this->expect(TokenKind::Else));

            this->next();
            auto else_expr = TRY(this->expr(false));

            Span span = { expr->span(), else_expr->span() };
            return { make<ast::TernaryExpr>(span, move(condition), move(expr), move(else_expr)) };
        }
        default:
            return expr;
    }
}

ParseResult<ast::Expr> Parser::call() {
    Span start = m_current.span();

    auto expr = TRY(this->primary());
    while (m_current.is(TokenKind::LParen)) {
        this->next();
        expr = TRY(this->parse_call(move(expr)));
    }

    if (m_current.is(TokenKind::Inc, TokenKind::Dec)) {
        UnaryOp op = m_current.kind() == TokenKind::Inc ? UnaryOp::Inc : UnaryOp::Dec;
        Span span = { start, m_current.span() };

        expr = make<ast::UnaryOpExpr>(span, move(expr), op);
        this->next();
    } else if (this->is_upcoming_constructor(*expr)) {
        this->next();

        Vector<ast::ConstructorArgument> arguments;
        if (m_current.is(TokenKind::RBrace)) {
            this->next();
            return { make<ast::EmptyConstructorExpr>(expr->span(), move(expr)) };
        }

        while (!m_current.is(TokenKind::RBrace)) {
            Token token = TRY(this->expect(TokenKind::Identifier));
            TRY(this->expect(TokenKind::Colon));

            auto value = TRY(this->expr(false));
            arguments.push_back({ token.value(), move(value), token.span() });

            if (!m_current.is(TokenKind::Comma)) {
                break;
            }

            this->next();
        }

        TRY(this->expect(TokenKind::RBrace));
        expr = make<ast::ConstructorExpr>(expr->span(), move(expr), move(arguments));
    }


    switch (m_current.kind()) {
        case TokenKind::Dot: 
            expr = TRY(this->attribute(move(expr))); break;
        case TokenKind::LBracket: 
            expr = TRY(this->index(move(expr))); break;
        default: break;
    }

    return expr;
}

ParseResult<ast::Expr> Parser::attribute(OwnPtr<ast::Expr> expr) {
    while (m_current.is(TokenKind::Dot)) {
        this->next();
        Token token = TRY(this->expect(TokenKind::Identifier));

        String value = token.value();
        Span span = { expr->span(), token.span() };

        expr = make<ast::AttributeExpr>(span, move(expr), value);
    }

    if (m_current.is(TokenKind::LBracket)) {
        return this->index(move(expr));
    }
    
    return expr;
}

ParseResult<ast::Expr> Parser::index(OwnPtr<ast::Expr> expr) {
    while (m_current.is(TokenKind::LBracket)) {
        this->next();
        auto index = TRY(this->expr(false));

        TRY(this->expect(TokenKind::RBracket));
        Span span = { expr->span(), m_current.span() };

        expr = make<ast::IndexExpr>(span, move(expr), move(index));
    }

    if (m_current.is(TokenKind::Dot)) {
        return this->attribute(move(expr));
    }

    return expr;
}

ParseResult<ast::Expr> Parser::primary() {
    OwnPtr<ast::Expr> expr = nullptr;
    Span start = m_current.span();

    switch (m_current.kind()) {
        case TokenKind::Integer: {
            String value = m_current.value();
            this->next();

            ast::IntegerSuffix suffix;

            // FIXME: Handle f32 and f64 correctly
            auto iterator = INTEGER_SUFFIXES.find(m_current.value());
            if (iterator != INTEGER_SUFFIXES.end() && m_current.is(TokenKind::Identifier)) {
                suffix = iterator->second;
                this->next();
            }

            // FIXME: This is ugly
            u16 base = DECIMAL_BASE;
            if (value.starts_with("0x")) {
                base = HEX_BASE;
                value = value.substr(2);
            } else if (value.starts_with("0b")) {
                base = BINARY_BASE;
                value = value.substr(2);
            }

            u64 result = std::strtoull(value.c_str(), nullptr, base);
            return { make<ast::IntegerExpr>(start, result, suffix) };
        }
        case TokenKind::Char: {
            String value = m_current.value();
            this->next();

            ast::IntegerSuffix suffix { ast::BuiltinType::i8 };
            return { make<ast::IntegerExpr>(start, value[0], suffix) };
        }
        case TokenKind::Float: {
            double result = 0.0;
            llvm::StringRef(m_current.value()).getAsDouble(result);

            this->next();
            bool is_double = false;

            if (m_current.value() == "f64") {
                this->next();
                is_double = true;
            }

            return { make<ast::FloatExpr>(start, result, is_double) };
        }
        case TokenKind::String: {
            String value = m_current.value();
            this->next();

            expr = make<ast::StringExpr>(start, value);
            break;
        }
        
        case TokenKind::True: BOOL_EXPR(True);
        case TokenKind::False: BOOL_EXPR(False);
        case TokenKind::Null: BOOL_EXPR(Null);

        case TokenKind::Identifier: {
            String name = m_current.value();
            this->next();

            if (m_current.is(TokenKind::DoubleColon)) {
                Path path = TRY(this->parse_path(name));
                Span span { start, m_current.span() };

                expr = make<ast::PathExpr>(span, move(path));
            } else {
                expr = make<ast::IdentifierExpr>(start, name);
            }

            break;
        }
        case TokenKind::Sizeof: {
            this->next();
            
            TRY(this->expect(TokenKind::LParen));
            OwnPtr<ast::Expr> expr = TRY(this->expr(false));

            TRY(this->expect(TokenKind::RParen));

            // TODO: sizeof for types
            return { make<ast::SizeofExpr>(start, move(expr)) };
        }
        case TokenKind::Offsetof: {
            this->next();

            TRY(this->expect(TokenKind::LParen));
            auto value = TRY(this->expr(false));

            TRY(this->expect(TokenKind::Comma));
            String field = TRY(this->expect(TokenKind::Identifier)).value();

            return { make<ast::OffsetofExpr>(start, move(value), field) };
        }
        case TokenKind::Match: {
            this->next();
            return this->parse_match();
        }
        case TokenKind::LParen: {
            this->next();

            if (m_current.is(TokenKind::Identifier) && this->peek().is(TokenKind::Colon)) {
                return this->parse_anonymous_function();
            }

            expr = TRY(this->expr(false));
            if (m_current.is(TokenKind::Comma)) {
                ast::ExprList<> elements;
                elements.push_back(move(expr));

                this->next();
                while (!m_current.is(TokenKind::RParen)) {
                    auto element = TRY(this->expr(false));
                    elements.push_back(move(element));

                    if (!m_current.is(TokenKind::Comma)) {
                        break;
                    }

                    this->next();
                }

                Span end = TRY(this->expect(TokenKind::RParen)).span();
                Span span = { start, end };

                expr = make<ast::TupleExpr>(span, move(elements));
            } else {
                TRY(this->expect(TokenKind::RParen));
            }
            break;
        }
        case TokenKind::LBracket: {
            this->next();
            ast::ExprList<> elements;

            if (!m_current.is(TokenKind::RBracket)) {
                auto element = TRY(this->expr(false));
                if (m_current.is(TokenKind::SemiColon)) {
                    this->next();
                    auto size = TRY(this->expr(false));

                    TRY(this->expect(TokenKind::RBracket));
                    Span span = { start, m_current.span() };

                    return { make<ast::ArrayFillExpr>(span, move(element), move(size)) };
                }

                elements.push_back(move(element));
                if (!m_current.is(TokenKind::RBracket)) {
                    TRY(this->expect(TokenKind::Comma));
                }
            }

            while (!m_current.is(TokenKind::RBracket)) {
                auto element = TRY(this->expr(false));
                elements.push_back(move(element));

                if (!m_current.is(TokenKind::Comma)) {
                    break;
                }

                this->next();
            }

            Span end = TRY(this->expect(TokenKind::RBracket)).span();
            Span span = { start, end };

            expr = make<ast::ArrayExpr>(span, move(elements));
            break;
        }
        case TokenKind::LBrace: {
            this->next();
            expr = TRY(this->parse_block());

            break;
        }
        default: {
            return err(m_current.span(), "Expected an expression");
        }
    }

    switch (m_current.kind()) {
        case TokenKind::Dot: 
            expr = TRY(this->attribute(move(expr))); break;
        case TokenKind::LBracket: 
            expr = TRY(this->index(move(expr))); break;
        default: break;
    }

    return expr;
}

}