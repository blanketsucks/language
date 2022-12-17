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
    Maybe,
    
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

    static Location dummy() { return Location { 0, 0, 0, "" }; }
};

struct Token {
    TokenKind type;
    Location start;
    Location end;
    std::string value;

    static std::string getTokenTypeValue(TokenKind type);

    bool match(TokenKind type, std::string value);
    bool match(TokenKind type, std::vector<std::string> values);
    bool match(std::vector<TokenKind> types);

    bool operator==(TokenKind type);
    bool operator==(Token token);
    bool operator!=(TokenKind type);
};

static std::vector<std::string> KEYWORDS = {
    // Keywords
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
    "typeof",
    "using",
    "from",
    "defer",
    "private",
    "enum",
    "where",
    "import",
    "foreach",
    "in",
    "static_assert",
    "immutable",
    "readonly",

    // Preprocessor keywords
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

    // Reserved words
    "__tuple"
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