#include "lexer/tokens.h"

std::string Token::get_type_value(TokenKind type) {
    switch (type) {
        case TokenKind::Inc: return "++";
        case TokenKind::Dec: return "--";
        case TokenKind::Add: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Mul: return "*";
        case TokenKind::Div: return "/";
        case TokenKind::Mod: return "%";
        case TokenKind::Not: return "!";
        case TokenKind::Or: return "|";
        case TokenKind::And: return "&";
        case TokenKind::BinaryOr: return "||";
        case TokenKind::BinaryAnd: return "&&";
        case TokenKind::BinaryNot: return "!";
        case TokenKind::Xor: return "^";
        case TokenKind::Rsh: return ">>";
        case TokenKind::Lsh: return "<<";
        case TokenKind::Eq: return "==";
        case TokenKind::Neq: return "!=";
        case TokenKind::Gt: return ">";
        case TokenKind::Lt: return "<";
        case TokenKind::Gte: return ">=";
        case TokenKind::Lte: return "<=";
        case TokenKind::Assign: return "=";
        default: return "";
    }
}

bool Token::match(TokenKind type, std::string value) {
    return this->type == type && this->value == value;
}

bool Token::match(TokenKind type, std::vector<std::string> values) {
    return this->type == type && (std::find(values.begin(), values.end(), this->value) != values.end());
}

bool Token::match(std::vector<TokenKind> types) {
    return std::find(types.begin(), types.end(), this->type) != types.end();
}

bool Token::operator==(TokenKind type) {
    return this->type == type;
}

bool Token::operator==(Token token) {
    return this->type == token.type && this->value == token.value;
}

bool Token::operator!=(TokenKind type) {
    return this->type != type;
}

