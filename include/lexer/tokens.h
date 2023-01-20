#ifndef _TOKENS_H
#define _TOKENS_H

#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include "lexer/location.h"

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
    Immutable,
    Readonly,
    Operator,

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
    
    EOS
};

struct Token {
    TokenKind type;
    std::string value;

    Span span;

    static std::string get_type_value(TokenKind type);

    bool match(TokenKind type, std::string value);
    bool match(TokenKind type, std::vector<std::string> values);
    bool match(std::vector<TokenKind> types);

    bool operator==(TokenKind type);
    bool operator==(Token token);
    bool operator!=(TokenKind type);
};

static std::map<std::string, TokenKind> KEYWORDS = {
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
    {"immutable", TokenKind::Immutable},
    {"readonly", TokenKind::Readonly},
    {"operator", TokenKind::Operator},

    // Reserved words
    {"__tuple", TokenKind::Reserved}
};

static std::vector<std::pair<TokenKind, int>> PRECEDENCES = {
    {TokenKind::Assign, 5},

    {TokenKind::Lt, 10},
    {TokenKind::Gt, 10},
    {TokenKind::Lte, 10},
    {TokenKind::Gte, 10},
    {TokenKind::Eq, 10},
    {TokenKind::Neq, 10},
    {TokenKind::And, 10},
    {TokenKind::Or, 10},

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

static std::vector<TokenKind> UNARY_OPERATORS = {
    TokenKind::Not,
    TokenKind::Add,
    TokenKind::Minus,
    TokenKind::BinaryNot,
    TokenKind::BinaryAnd,
    TokenKind::Mul,
    TokenKind::Inc,
    TokenKind::Dec,
};

static std::map<TokenKind, TokenKind> INPLACE_OPERATORS {
    {TokenKind::IAdd, TokenKind::Add},
    {TokenKind::IMinus, TokenKind::Minus},
    {TokenKind::IMul, TokenKind::Mul},
    {TokenKind::IDiv, TokenKind::Div},
    // TODO: add more
};  


#endif