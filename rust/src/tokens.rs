use std::collections::HashMap;
use lazy_static::lazy_static;

#[derive(Debug, PartialEq, Hash, Eq, Clone)]
pub enum TokenKind {
    Identifier(String),
    Number(String),
    String(String),
    Char(char),

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
    Shr,
    Shl,

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
    Range, // ..
    
    EOF
}

impl TokenKind {
    pub fn is_inplace_binary_op(&self) -> bool {
        matches!(self, TokenKind::IAdd | TokenKind::IMinus | TokenKind::IMul | TokenKind::IDiv)
    }

    pub fn precedence(&self) -> i8 {
        return match PRECEDENCES.get(self) {
            Some(p) => *p as i8,
            None => -1
        }
    }
}

#[derive(Debug, PartialEq, Clone, Default)]
pub struct Location {
    pub line: usize,
    pub column: usize,
    pub index: usize,
}

#[derive(Debug, PartialEq, Clone, Default)]
pub struct Span {
    pub start: Location,
    pub end: Location,
    pub line: String,
    pub filename: String,
}

impl Span {
    pub fn from_span(start: Span, end: Span) -> Self {
        Self {
            start: start.start,
            end: end.end,
            line: start.line,
            filename: start.filename
        }
    }

    pub fn length(&self) -> usize {
        self.end.index - self.start.index
    }
}

#[derive(Debug, PartialEq, Clone)]
pub struct Token {
    pub kind: TokenKind,
    pub span: Span
}

impl Token {
    pub fn new(kind: TokenKind, span: Span) -> Self {
        Self { kind, span }
    }
}

macro_rules! span {
    ($start:expr, $end:expr) => {
        Span::from_span($start.clone(), $end.clone())
    };
}

pub(crate) use span;

lazy_static! {
    pub static ref KEYWORDS: HashMap<&'static str, TokenKind> = {
        [
            ("extern", TokenKind::Extern),
            ("func", TokenKind::Func),
            ("return", TokenKind::Return),
            ("if", TokenKind::If),
            ("else", TokenKind::Else),
            ("while", TokenKind::While),
            ("for", TokenKind::For),
            ("break", TokenKind::Break),
            ("continue", TokenKind::Continue),
            ("let", TokenKind::Let),
            ("const", TokenKind::Const),
            ("struct", TokenKind::Struct),
            ("namespace", TokenKind::Namespace),
            ("enum", TokenKind::Enum),
            ("module", TokenKind::Module),
            ("import", TokenKind::Import),
            ("as", TokenKind::As),
            ("type", TokenKind::Type),
            ("sizeof", TokenKind::Sizeof),
            ("offsetof", TokenKind::Offsetof),
            ("typeof", TokenKind::Typeof),
            ("using", TokenKind::Using),
            ("from", TokenKind::From),
            ("defer", TokenKind::Defer),
            ("private", TokenKind::Private),
            ("foreach", TokenKind::Foreach),
            ("in", TokenKind::In),
            ("static_assert", TokenKind::StaticAssert),
            ("mut", TokenKind::Mut),
            ("readonly", TokenKind::Readonly),
            ("operator", TokenKind::Operator),
            ("impl", TokenKind::Impl),
        ].into()
    };

    pub static ref PRECEDENCES: HashMap<TokenKind, u8> = {
        [
            (TokenKind::Assign, 5),

            (TokenKind::And, 10),
            (TokenKind::Or, 10),

            (TokenKind::Lt, 15),
            (TokenKind::Gt, 15),
            (TokenKind::Lte, 15),
            (TokenKind::Gte, 15),
            (TokenKind::Eq, 15),
            (TokenKind::Neq, 15),

            (TokenKind::BinaryAnd, 20),
            (TokenKind::BinaryOr, 20),
            (TokenKind::Xor, 20),
            (TokenKind::Shr, 20),
            (TokenKind::Shl, 20),
        
            (TokenKind::IAdd, 25),
            (TokenKind::IMinus, 25),
            (TokenKind::IMul, 25),
            (TokenKind::IDiv, 25),
        
            (TokenKind::Add, 30),
            (TokenKind::Minus, 30),
            (TokenKind::Mod, 35),
            (TokenKind::Div, 40),
            (TokenKind::Mul, 40)
        ].into()
    };
}
