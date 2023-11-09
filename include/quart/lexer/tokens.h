#pragma once

#include <quart/lexer/location.h>

#include <llvm/ADT/StringRef.h>

#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <algorithm>

namespace quart {

enum class TokenKind;

bool is_keyword(llvm::StringRef word);
TokenKind get_keyword_kind(llvm::StringRef word);

enum class TokenKind {
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

    Reserved,

    Add,
    Minus,
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
    IMinus,
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
    DoubleArrow, // =>
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

struct Token {
    TokenKind type;
    std::string value;

    Span span;

    static llvm::StringRef get_type_value(TokenKind type);

    inline bool is_keyword() const { return quart::is_keyword(this->value); }

    bool is(TokenKind type) const {
        return this->type == type;
    }

    bool is(const std::vector<TokenKind>& types) const { 
        return std::find(types.begin(), types.end(), this->type) != types.end();
    }

    template<typename... Args> bool is(TokenKind type, Args... args) const { 
        return this->is(type) || this->is(args...);
    }

    bool operator==(TokenKind type) const;
    bool operator==(const Token& token) const;
    bool operator!=(TokenKind type) const;
};

static std::map<llvm::StringRef, TokenKind> KEYWORDS = {
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

    // Reserved words
    {"__tuple", TokenKind::Reserved}
};

static std::map<TokenKind, int> PRECEDENCES = {
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
    {TokenKind::IMinus, 25},
    {TokenKind::IMul, 25},
    {TokenKind::IDiv, 25},

    {TokenKind::Add, 30},
    {TokenKind::Minus, 30},
    {TokenKind::Mod, 35},
    {TokenKind::Div, 40},
    {TokenKind::Mul, 40}
};

static std::map<TokenKind, UnaryOp> UNARY_OPS = {
    {TokenKind::Not, UnaryOp::Not},
    {TokenKind::Add, UnaryOp::Add},
    {TokenKind::Minus, UnaryOp::Sub},
    {TokenKind::BinaryNot, UnaryOp::BinaryNot},
    {TokenKind::BinaryAnd, UnaryOp::BinaryAnd},
    {TokenKind::Mul, UnaryOp::Mul},
    {TokenKind::Inc, UnaryOp::Inc},
    {TokenKind::Dec, UnaryOp::Dec}
};

static std::map<TokenKind, BinaryOp> BINARY_OPS = {
    {TokenKind::Add, BinaryOp::Add},
    {TokenKind::Minus, BinaryOp::Sub},
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
    {TokenKind::Assign, BinaryOp::Assign}
};

static std::map<TokenKind, BinaryOp> INPLACE_OPERATORS {
    {TokenKind::IAdd, BinaryOp::Add},
    {TokenKind::IMinus, BinaryOp::Sub},
    {TokenKind::IMul, BinaryOp::Mul},
    {TokenKind::IDiv, BinaryOp::Div},
    // TODO: add more
};  

}