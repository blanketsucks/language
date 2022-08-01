#include "tokens.h"

std::string Location::format() { 
    std::string line = std::to_string(this->line);
    std::string column = std::to_string(this->column);

    return this->filename + ":" + line + ":" + column;
}

std::string Token::getTokenTypeValue(TokenType type) {
    switch (type) {
        case TokenType::Inc: return "++";
        case TokenType::Dec: return "--";
        case TokenType::Add: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Mul: return "*";
        case TokenType::Div: return "/";
        case TokenType::Not: return "!";
        case TokenType::Or: return "|";
        case TokenType::And: return "&";
        case TokenType::BinaryOr: return "||";
        case TokenType::BinaryAnd: return "&&";
        case TokenType::BinaryNot: return "!";
        case TokenType::Xor: return "^";
        case TokenType::Rsh: return ">>";
        case TokenType::Lsh: return "<<";
        case TokenType::Eq: return "==";
        case TokenType::Neq: return "!=";
        case TokenType::Gt: return ">";
        case TokenType::Lt: return "<";
        case TokenType::Gte: return ">=";
        case TokenType::Lte: return "<=";
        case TokenType::Assign: return "=";
        default: return "";
    }
}

bool Token::operator==(TokenType type) {
    return this->type == type;
}

bool Token::operator==(Token token) {
    return this->type == token.type && this->value == token.value;
}

bool Token::operator!=(TokenType type) {
    return this->type != type;
}

