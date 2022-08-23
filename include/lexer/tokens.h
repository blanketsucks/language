#ifndef _TOKENS_H
#define _TOKENS_H

#include <vector>
#include <iostream>
#include <string>
#include <map>
#include <algorithm>

enum class TokenKind {
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

    Location update(uint32_t line, uint32_t column, uint32_t index);
    Location update(uint32_t column, uint32_t index);

    std::string format();
};

struct Token {
    TokenKind type;
    Location start;
    Location end;
    std::string value;

    static std::string getTokenTypeValue(TokenKind type);

    bool match(TokenKind type, std::string value);
    bool match(TokenKind type, std::vector<std::string> values);

    bool operator==(TokenKind type);
    bool operator==(Token token);
    bool operator!=(TokenKind type);
};

static std::vector<std::string> KEYWORDS = {
    "extern",
    "func",
    "return",
    "if",
    "else",
    "while",
    "for",
    "break",
    "continue",
    "let",
    "const",
    "struct",
    "namespace",
    "type",
    "as",
    "sizeof",
    "offsetof",
    "using",
    "from",
    "defer",
    "private",

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

static std::vector<std::pair<TokenKind, int>> PRECEDENCES = {
    std::make_pair(TokenKind::Assign, 5),

    std::make_pair(TokenKind::Lt, 10),
    std::make_pair(TokenKind::Gt, 10),
    std::make_pair(TokenKind::Lte, 10),
    std::make_pair(TokenKind::Gte, 10),
    std::make_pair(TokenKind::Eq, 10),
    std::make_pair(TokenKind::Neq, 10),
    std::make_pair(TokenKind::And, 10),
    std::make_pair(TokenKind::Or, 10),

    std::make_pair(TokenKind::BinaryAnd, 20),
    std::make_pair(TokenKind::BinaryOr, 20),
    std::make_pair(TokenKind::Xor, 20),
    std::make_pair(TokenKind::Rsh, 20),
    std::make_pair(TokenKind::Lsh, 20),

    std::make_pair(TokenKind::IAdd, 25),
    std::make_pair(TokenKind::IMinus, 25),
    std::make_pair(TokenKind::IMul, 25),
    std::make_pair(TokenKind::IDiv, 25),

    std::make_pair(TokenKind::Add, 30),
    std::make_pair(TokenKind::Minus, 30),
    std::make_pair(TokenKind::Mod, 35),
    std::make_pair(TokenKind::Div, 40),
    std::make_pair(TokenKind::Mul, 40)
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
};  


#endif