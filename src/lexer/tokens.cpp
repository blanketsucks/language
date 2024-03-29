#include <quart/lexer/tokens.h>

using namespace quart;

bool quart::is_keyword(llvm::StringRef word) {
    return KEYWORDS.find(word) != KEYWORDS.end();
}

TokenKind quart::get_keyword_kind(llvm::StringRef word) {
    auto it = KEYWORDS.find(word);
    if (it == KEYWORDS.end()) return TokenKind::EOS;

    return it->second;
}

llvm::StringRef quart::get_unary_op_value(UnaryOp type) {
    switch (type) {
        case UnaryOp::Inc: return "++";
        case UnaryOp::Dec: return "--";
        case UnaryOp::Add: return "+";
        case UnaryOp::Sub: return "-";
        case UnaryOp::Not: return "!";
        case UnaryOp::BinaryNot: return "~";
        case UnaryOp::BinaryAnd: return "&";
        case UnaryOp::Mul: return "*";
    }

    return "";
}

llvm::StringRef quart::get_binary_op_value(BinaryOp type) {
    switch (type) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Or: return "|";
        case BinaryOp::And: return "&";
        case BinaryOp::BinaryOr: return "||";
        case BinaryOp::BinaryAnd: return "&&";
        case BinaryOp::Xor: return "^";
        case BinaryOp::Rsh: return ">>";
        case BinaryOp::Lsh: return "<<";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Neq: return "!=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Gte: return ">=";
        case BinaryOp::Lte: return "<=";
        case BinaryOp::Assign: return "=";
        default: return "";
    }
}

llvm::StringRef Token::get_type_value(TokenKind type) {
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

i8 Token::precedence() const {
    auto iterator = PRECEDENCES.find(this->type);
    if (iterator == PRECEDENCES.end()) return -1;

    return static_cast<i8>(iterator->second);
}

bool Token::operator==(TokenKind type) const {
    return this->type == type;
}

bool Token::operator==(const Token& token) const {
    return this->type == token.type && this->value == token.value;
}

bool Token::operator!=(TokenKind type) const {
    return this->type != type;
}

