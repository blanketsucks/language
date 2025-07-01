#include <quart/lexer/tokens.h>

namespace quart {

bool is_comparison_operator(BinaryOp op) {
    switch (op) {
        case BinaryOp::LogicalAnd:
        case BinaryOp::LogicalOr:
        case BinaryOp::Eq:
        case BinaryOp::Neq:
        case BinaryOp::Gt:
        case BinaryOp::Lt:
        case BinaryOp::Gte:
        case BinaryOp::Lte:
            return true;
        default:
            return false;
    }
}

bool is_keyword(StringView word) {
    return KEYWORDS.find(word) != KEYWORDS.end();
}

TokenKind get_keyword_kind(StringView word) {
    auto it = KEYWORDS.find(word);
    if (it == KEYWORDS.end()) {
        return TokenKind::EOS;
    }

    return it->second;
}

StringView get_unary_op_value(UnaryOp type) {
    switch (type) {
        case UnaryOp::Inc: return "++";
        case UnaryOp::Dec: return "--";
        case UnaryOp::Add: return "+";
        case UnaryOp::Neg: return "-";
        case UnaryOp::Not: return "!";
        case UnaryOp::BinaryNeg: return "~";
        case UnaryOp::Ref: return "&";
        case UnaryOp::DeRef: return "*";
    }

    return {};
}

StringView get_binary_op_value(BinaryOp type) {
    switch (type) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::Or: return "|";
        case BinaryOp::And: return "&";
        case BinaryOp::LogicalOr: return "||";
        case BinaryOp::LogicalAnd: return "&&";
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
    }

    return {};
}

StringView token_kind_to_str(TokenKind kind) {
    switch (kind) {
        case TokenKind::Identifier: return "identifier";
        case TokenKind::Integer: return "integer";
        case TokenKind::Float: return "float";
        case TokenKind::String: return "string";
        case TokenKind::Char: return "char";
        case TokenKind::Extern: return "extern";
        case TokenKind::Func: return "func";
        case TokenKind::Return: return "return";
        case TokenKind::If: return "if";
        case TokenKind::Else: return "else";
        case TokenKind::While: return "while";
        case TokenKind::For: return "for";
        case TokenKind::Break: return "break";
        case TokenKind::Continue: return "continue";
        case TokenKind::Let: return "let";
        case TokenKind::Const: return "const";
        case TokenKind::Struct: return "struct";
        case TokenKind::Namespace: return "namespace";
        case TokenKind::Enum: return "enum";
        case TokenKind::Module: return "module";
        case TokenKind::Import: return "import";
        case TokenKind::As: return "as";
        case TokenKind::Type: return "type";
        case TokenKind::Sizeof: return "sizeof";
        case TokenKind::Offsetof: return "offsetof";
        case TokenKind::Typeof: return "typeof";
        case TokenKind::Using: return "using";
        case TokenKind::Defer: return "defer";
        case TokenKind::Pub: return "pub";
        case TokenKind::Foreach: return "foreach";
        case TokenKind::In: return "in";
        case TokenKind::StaticAssert: return "static_assert";
        case TokenKind::Mut: return "mut";
        case TokenKind::Readonly: return "readonly";
        case TokenKind::Operator: return "operator";
        case TokenKind::Impl: return "impl";
        case TokenKind::Match: return "match";
        case TokenKind::Add: return "+";
        case TokenKind::Sub: return "-";
        case TokenKind::Mul: return "*";
        case TokenKind::Div: return "/";
        case TokenKind::Mod: return "%";
        case TokenKind::Not: return "!";
        case TokenKind::LogicalOr: return "||";
        case TokenKind::LogicalAnd: return "&&";
        case TokenKind::Inc: return "++";
        case TokenKind::Dec: return "--";
        case TokenKind::Or: return "|";
        case TokenKind::And: return "&";
        case TokenKind::BinaryNot: return "~";
        case TokenKind::Xor: return "^";
        case TokenKind::Rsh: return ">>";
        case TokenKind::Lsh: return "<<";
        case TokenKind::IAdd: return "+=";
        case TokenKind::ISub: return "-=";
        case TokenKind::IMul: return "*=";
        case TokenKind::IDiv: return "/=";
        case TokenKind::IOr: return "|=";
        case TokenKind::IAnd: return "&=";
        case TokenKind::IXor: return "^=";
        case TokenKind::IRsh: return ">>=";
        case TokenKind::ILsh: return "<<=";
        case TokenKind::Eq: return "==";
        case TokenKind::Neq: return "!=";
        case TokenKind::Gt: return ">";
        case TokenKind::Lt: return "<";
        case TokenKind::Gte: return ">=";
        case TokenKind::Lte: return "<=";
        case TokenKind::Assign: return "=";
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::Comma: return ",";
        case TokenKind::SemiColon: return ";";
        case TokenKind::Colon: return ":";
        case TokenKind::Dot: return ".";
        case TokenKind::DoubleColon: return "::";
        case TokenKind::Arrow: return "->";
        case TokenKind::FatArrow: return "=>";
        case TokenKind::Ellipsis: return "...";
        case TokenKind::Newline: return "newline";
        case TokenKind::Maybe: return "?";
        case TokenKind::DoubleDot: return "..";
        case TokenKind::EOS: return "EOS";
        case TokenKind::None: return "None";
        case TokenKind::Trait: return "trait";
    }

    return {};
}

i8 Token::precedence() const {
    auto iterator = PRECEDENCES.find(m_kind);
    if (iterator == PRECEDENCES.end()) {
        return -1;
    }

    return static_cast<i8>(iterator->second);
}

}