#ifndef _TOKENS_HPP
#define _TOKENS_HPP

#include <vector>
#include <iostream>
#include <map>

enum class TokenType {
    IDENTIFIER,
    KEYWORD,
    INTEGER,
    FLOAT,
    STRING,

    // Operators
    PLUS,
    MINUS,
    MUL,
    DIV,
    NOT,
    OR,
    AND,
    INC,
    DEC,

    // Binary Operators
    BINARY_OR,
    BINARY_AND,
    BINARY_NOT,
    XOR,
    RSH,
    LSH,

    // Comparison
    EQ,
    NEQ,
    GT,
    LT,
    GTE,
    LTE,

    // Assignment
    ASSIGN,

    // Other
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    SEMICOLON,
    COLON,
    DOT,
    DOUBLECOLON,
    ARROW,
    ELLIPSIS,
    EOS
};

struct Location {
    int column;
    int line;
    int index;
    std::string filename;

    Location(int column = 0, int line = 0, int index = 0, std::string filename = "") :
        column(column),
        line(line),
        index(index),
        filename(filename)
    {}

    std::string format() { return this->filename + ":" + std::to_string(this->line); }
};

struct Token {
    TokenType type;
    Location start;
    Location end;
    std::string value;

    std::string to_string() {
        switch (this->type) {
            case TokenType::IDENTIFIER: return "IDENT";
            case TokenType::KEYWORD: return "KEYWORD";
            case TokenType::INTEGER: return "INTEGER";
            case TokenType::FLOAT: return "FLOAT";
            case TokenType::STRING: return "STRING";
            case TokenType::PLUS: return "PLUS";
            case TokenType::MINUS: return "MINUS";
            case TokenType::MUL: return "MUL";
            case TokenType::DIV: return "DIV";
            case TokenType::NOT: return "NOT";
            case TokenType::OR: return "OR";
            case TokenType::AND: return "AND";
            case TokenType::BINARY_OR: return "BINARY_OR";
            case TokenType::BINARY_AND: return "BINARY_AND";
            case TokenType::BINARY_NOT: return "BINARY_NOT";
            case TokenType::XOR: return "XOR";
            case TokenType::RSH: return "RSH";
            case TokenType::LSH: return "LSH";
            case TokenType::EQ: return "EQ";
            case TokenType::NEQ: return "NEQ";
            case TokenType::GT: return "GT";
            case TokenType::LT: return "LT";
            case TokenType::GTE: return "GTE";
            case TokenType::LTE: return "LTE";
            case TokenType::ASSIGN: return "ASSIGN";
            case TokenType::LPAREN: return "LPAREN";
            case TokenType::RPAREN: return "RPAREN";
            case TokenType::LBRACE: return "LBRACE";
            case TokenType::RBRACE: return "RBRACE";
            case TokenType::LBRACKET: return "LBRACKET";
            case TokenType::RBRACKET: return "RBRACKET";
            case TokenType::COMMA: return "COMMA";
            case TokenType::SEMICOLON: return "SEMICOLON";
            case TokenType::COLON: return "COLON";
            case TokenType::DOT: return "DOT";
            case TokenType::DOUBLECOLON: return "DOUBLECOLON";
            case TokenType::ELLIPSIS: return "ELLIPSIS";
            case TokenType::EOS: return "EOS";
        }
    }

    bool operator==(TokenType type) {
        return this->type == type;
    }

    bool operator!=(TokenType type) {
        return this->type != type;
    }
};

static std::vector<std::string> KEYWORDS = {
    "extern",
    "def",
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
};

static std::vector<std::pair<TokenType, int>> PRECEDENCES = {
    // Assignment operator - 5 precedence.
    std::make_pair(TokenType::ASSIGN, 5),

    // Comparison operators - 10 precedence.
    std::make_pair(TokenType::LT, 10),
    std::make_pair(TokenType::GT, 10),
    std::make_pair(TokenType::LTE, 10),
    std::make_pair(TokenType::GTE, 10),
    std::make_pair(TokenType::EQ, 10),
    std::make_pair(TokenType::NEQ, 10),
    std::make_pair(TokenType::AND, 10),
    std::make_pair(TokenType::OR, 10),

    // Binary operators - 20 precedence.
    std::make_pair(TokenType::BINARY_AND, 20),
    std::make_pair(TokenType::BINARY_OR, 20),
    std::make_pair(TokenType::XOR, 20),
    std::make_pair(TokenType::RSH, 20),
    std::make_pair(TokenType::LSH, 20),

    // Arithmic operators - from 30 to 40.
    std::make_pair(TokenType::PLUS, 30),
    std::make_pair(TokenType::MINUS, 30),
    std::make_pair(TokenType::DIV, 40),
    std::make_pair(TokenType::MUL, 40)
};

static std::vector<TokenType> UNARY_OPERATORS = {
    TokenType::NOT,
    TokenType::PLUS,
    TokenType::MINUS,
    TokenType::BINARY_NOT,
    TokenType::BINARY_AND,
    TokenType::MUL,
    TokenType::INC,
    TokenType::DEC,
};


#endif