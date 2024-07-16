#pragma once

#include <quart/source_code.h>

#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/STLExtras.h>

#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#define ENUMERATE_BINARY_OPS(OP) \
    OP(Add)                      \
    OP(Sub)                      \
    OP(Mul)                      \
    OP(Div)                      \
    OP(Mod)                      \
    OP(Or)                       \
    OP(And)                      \
    OP(BinaryOr)                 \
    OP(BinaryAnd)                \
    OP(Xor)                      \
    OP(Rsh)                      \
    OP(Lsh)                      \
    OP(Eq)                       \
    OP(Neq)                      \
    OP(Gt)                       \
    OP(Lt)                       \
    OP(Gte)                      \
    OP(Lte)                      \

namespace quart {

enum class TokenKind;

bool is_keyword(StringView value);
TokenKind get_keyword_kind(StringView word);

enum class TokenKind {
    None,

    Identifier,
    Integer,
    Float,
    String,
    Char,

    // Keywords
    Extern,
    Func,
    Return,
    If,
    Else,
    While,
    For,
    Break,
    Continue,
    Let,
    Const,
    Struct,
    Namespace,
    Enum,
    Module,
    Import,
    As,
    Type,
    Sizeof,
    Offsetof,
    Typeof,
    Using,
    From,
    Defer,
    Private,
    Foreach,
    In,
    StaticAssert,
    Mut,
    Readonly,
    Operator,
    Impl,
    Match,

    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Not,
    Or,
    And,
    Inc,
    Dec,

    BinaryOr,
    BinaryAnd,
    BinaryNot,
    Xor,
    Rsh,
    Lsh,

    IAdd,
    ISub,
    IMul,
    IDiv,

    Eq,
    Neq,
    Gt,
    Lt,
    Gte,
    Lte,

    Assign,

    LParen,
    RParen,
    LBrace,
    RBrace,
    LBracket,
    RBracket,
    Comma,
    SemiColon,
    Colon,
    Dot,
    DoubleColon,
    Arrow, // ->
    FatArrow, // =>
    Ellipsis,
    Newline,
    Maybe,
    DoubleDot, // ..
    
    EOS
};

enum class UnaryOp {
    Not,
    Add,
    Sub,
    BinaryNot,
    BinaryAnd,
    Mul,
    Inc,
    Dec
};

enum class BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    Or,
    And,
    BinaryOr,
    BinaryAnd,
    Xor,
    Rsh,
    Lsh,
    Eq,
    Neq,
    Gt,
    Lt,
    Gte,
    Lte,
    Assign
};

llvm::StringRef get_binary_op_value(BinaryOp op);
llvm::StringRef get_unary_op_value(UnaryOp op);

StringView token_kind_to_str(TokenKind);

class Token {
public:
    Token() = default;
    Token(TokenKind kind, String value, Span span) : m_kind(kind), m_value(move(value)), m_span(span) {}

    TokenKind kind() const { return m_kind; }
    String const& value() const { return m_value; }

    Span span() const { return m_span; }

    inline bool is_keyword() const { return quart::is_keyword(m_value); }

    bool is(TokenKind kind) const {
        return m_kind == kind;
    }
    
    template<typename ...Args> requires(of_type_v<TokenKind, Args...>)
    bool is(TokenKind kind, Args... args) const {
        return this->is(kind) || this->is(args...);
    }

    i8 precedence() const;

private:
    TokenKind m_kind = TokenKind::None;
    String m_value;

    Span m_span;
};

static const std::map<llvm::StringRef, TokenKind> KEYWORDS = {
    // Keywords
    {"extern", TokenKind::Extern},
    {"func", TokenKind::Func},
    {"return", TokenKind::Return},
    {"if", TokenKind::If},
    {"else", TokenKind::Else},
    {"while", TokenKind::While},
    {"for", TokenKind::For},
    {"break", TokenKind::Break},
    {"continue", TokenKind::Continue},
    {"let", TokenKind::Let},
    {"const", TokenKind::Const},
    {"struct", TokenKind::Struct},
    {"namespace", TokenKind::Namespace},
    {"enum", TokenKind::Enum},
    {"module", TokenKind::Module},
    {"import", TokenKind::Import},
    {"as", TokenKind::As},
    {"type", TokenKind::Type},
    {"sizeof", TokenKind::Sizeof},
    {"offsetof", TokenKind::Offsetof},
    {"typeof", TokenKind::Typeof},
    {"using", TokenKind::Using},
    {"from", TokenKind::From},
    {"defer", TokenKind::Defer},
    {"private", TokenKind::Private},
    {"foreach", TokenKind::Foreach},
    {"in", TokenKind::In},
    {"static_assert", TokenKind::StaticAssert},
    {"mut", TokenKind::Mut},
    {"readonly", TokenKind::Readonly},
    {"operator", TokenKind::Operator},
    {"impl", TokenKind::Impl},
    {"match", TokenKind::Match},
};

static const std::map<TokenKind, u8> PRECEDENCES = {
    {TokenKind::Assign, 5},

    {TokenKind::And, 10},
    {TokenKind::Or, 10},
    {TokenKind::Lt, 15},
    {TokenKind::Gt, 15},
    {TokenKind::Lte, 15},
    {TokenKind::Gte, 15},
    {TokenKind::Eq, 15},
    {TokenKind::Neq, 15},

    {TokenKind::BinaryAnd, 20},
    {TokenKind::BinaryOr, 20},
    {TokenKind::Xor, 20},
    {TokenKind::Rsh, 20},
    {TokenKind::Lsh, 20},

    {TokenKind::IAdd, 25},
    {TokenKind::ISub, 25},
    {TokenKind::IMul, 25},
    {TokenKind::IDiv, 25},

    {TokenKind::Add, 30},
    {TokenKind::Sub, 30},
    {TokenKind::Mod, 35},
    {TokenKind::Div, 40},
    {TokenKind::Mul, 40}
};

static const std::map<TokenKind, UnaryOp> UNARY_OPS = {
    {TokenKind::Not, UnaryOp::Not},
    {TokenKind::Add, UnaryOp::Add},
    {TokenKind::Sub, UnaryOp::Sub},
    {TokenKind::BinaryNot, UnaryOp::BinaryNot},
    {TokenKind::BinaryAnd, UnaryOp::BinaryAnd},
    {TokenKind::Mul, UnaryOp::Mul},
    {TokenKind::Inc, UnaryOp::Inc},
    {TokenKind::Dec, UnaryOp::Dec}
};

static const std::map<TokenKind, BinaryOp> BINARY_OPS = {
    {TokenKind::Add, BinaryOp::Add},
    {TokenKind::Sub, BinaryOp::Sub},
    {TokenKind::Mul, BinaryOp::Mul},
    {TokenKind::Div, BinaryOp::Div},
    {TokenKind::Mod, BinaryOp::Mod},
    {TokenKind::Or, BinaryOp::Or},
    {TokenKind::And, BinaryOp::And},
    {TokenKind::BinaryOr, BinaryOp::BinaryOr},
    {TokenKind::BinaryAnd, BinaryOp::BinaryAnd},
    {TokenKind::Xor, BinaryOp::Xor},
    {TokenKind::Rsh, BinaryOp::Rsh},
    {TokenKind::Lsh, BinaryOp::Lsh},
    {TokenKind::Eq, BinaryOp::Eq},
    {TokenKind::Neq, BinaryOp::Neq},
    {TokenKind::Gt, BinaryOp::Gt},
    {TokenKind::Lt, BinaryOp::Lt},
    {TokenKind::Gte, BinaryOp::Gte},
    {TokenKind::Lte, BinaryOp::Lte},
    {TokenKind::Assign, BinaryOp::Assign},
    {TokenKind::IAdd, BinaryOp::Add},
    {TokenKind::ISub, BinaryOp::Sub},
    {TokenKind::IMul, BinaryOp::Mul},
    {TokenKind::IDiv, BinaryOp::Div}
};

static const std::map<TokenKind, BinaryOp> INPLACE_OPERATORS {
    {TokenKind::IAdd, BinaryOp::Add},
    {TokenKind::ISub, BinaryOp::Sub},
    {TokenKind::IMul, BinaryOp::Mul},
    {TokenKind::IDiv, BinaryOp::Div},
    // TODO: add more
};  

}

namespace llvm {

template<>
struct format_provider<quart::Token> {
    static void format(const quart::Token& token, raw_ostream& stream, StringRef) {
        stream << "Token { kind: " << int(token.kind()) << ", value: '" << token.value() << "' }";
    }
};

}