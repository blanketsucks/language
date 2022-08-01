#ifndef _TOKENS_H
#define _TOKENS_H

#include <vector>
#include <iostream>
#include <string>
#include <map>

enum class TokenType {
    Identifier,
    Keyword,
    Integer,
    Float,
    String,
    Char,

    Add,
    Minus,
    Mul,
    Div,
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
    Arrow,
    Ellipsis,
    Newline,
    
    EOS
};

struct Location {
    uint32_t line;
    uint32_t column;
    uint32_t index;

    std::string filename;

    std::string format();
};

struct Token {
    TokenType type;
    Location start;
    Location end;
    std::string value;

    static std::string getTokenTypeValue(TokenType type);

    bool operator==(TokenType type);
    bool operator==(Token token);
    bool operator!=(TokenType type);
};

static std::vector<std::string> KEYWORDS = {
    "extern",
    "func",
    "return",
    "if",
    "else",
    "while",
    "let",
    "const",
    "struct",
    "packed",
    "include",
    "namespace",
    "type",
    "in",
    "as",
    "sizeof",
    "asm",
    "static_assert",
    "$define",
    "$undef",
    "$error",
    "$include",
    "$ifdef",
    "$ifndef",
    "$endif",
    "$if",
    "$elif",
    "$else",
};

static std::vector<std::pair<TokenType, int>> PRECEDENCES = {
    std::make_pair(TokenType::Assign, 5),

    std::make_pair(TokenType::Lt, 10),
    std::make_pair(TokenType::Gt, 10),
    std::make_pair(TokenType::Lte, 10),
    std::make_pair(TokenType::Gte, 10),
    std::make_pair(TokenType::Eq, 10),
    std::make_pair(TokenType::Neq, 10),
    std::make_pair(TokenType::And, 10),
    std::make_pair(TokenType::Or, 10),

    std::make_pair(TokenType::BinaryAnd, 20),
    std::make_pair(TokenType::BinaryOr, 20),
    std::make_pair(TokenType::Xor, 20),
    std::make_pair(TokenType::Rsh, 20),
    std::make_pair(TokenType::Lsh, 20),

    std::make_pair(TokenType::Add, 30),
    std::make_pair(TokenType::Minus, 30),
    std::make_pair(TokenType::Div, 40),
    std::make_pair(TokenType::Mul, 40)
};

static std::vector<TokenType> UNARY_OPERATORS = {
    TokenType::Not,
    TokenType::Add,
    TokenType::Minus,
    TokenType::BinaryNot,
    TokenType::BinaryAnd,
    TokenType::Mul,
    TokenType::Inc,
    TokenType::Dec,
};


#endif