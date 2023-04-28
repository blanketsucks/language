use std::str::Chars;

use crate::tokens::{Token, Span, TokenKind, Location, KEYWORDS};
use crate::log::error;

pub struct Lexer<'a> {
    source: &'a String,
    filename: &'a String,

    buffer: Chars<'a>,
    current: char,

    line: usize,
    column: usize,
    index: usize,
    eof: bool
}

impl Lexer<'_> {
    pub fn new<'a>(filename: &'a String, buffer: &'a String) -> Lexer<'a> {
        let mut lexer = Lexer {
            source: buffer,
            filename,
            buffer: buffer.chars(),
            current: '\0',
            line: 1,
            column: 1,
            index: 0,
            eof: false
        };

        lexer.next();
        lexer
    }

    fn next(&mut self) {
        self.current = match self.buffer.next() {
            Some(c) => c,
            None => {
                self.eof = true;
                '\0'
            }
        };

        if self.index == usize::MAX {
            error!(&self.make_span(self.loc(), self.loc()), "Lexer index overflow. File is too big.");
        } else if self.column == usize::MAX {
            error!(&self.make_span(self.loc(), self.loc()), "Lexer column overflow. File is too big.");
        }

        self.index += 1;
        self.column += 1;
    }

    fn loc(&self) -> Location {
        Location {
            line: self.line,
            column: self.column,
            index: self.index
        }
    }

    fn make_span(&self, start: Location, end: Location) -> Span {
        let mut offset = 0;
        if start.index > start.column {
            offset = start.index - start.column;
        }

        let mut line = String::new();
        if offset < self.source.len() {
            let mut str = &self.source[offset..];
            if str.starts_with('\n') {
                str = &str[1..];
            }
            
            match str.find('\n') {
                Some(index) => line = str[..index].to_string(),
                None => line = str.to_string()
            }
        }

        Span {
            start,
            end,
            line,
            filename: self.filename.clone()
        }
    }

    fn expect_or(&mut self, ch: char, expected: TokenKind, or: TokenKind) -> TokenKind {
        if self.current == ch {
            self.next();
            expected
        } else {
            or
        }
    }

    fn skip_whitespace(&mut self) {
        while self.current.is_whitespace() {
            if self.current == '\n' {
                self.line += 1;
                self.column = 1;
            }

            self.next();
        }
    }

    fn skip_comment(&mut self) {
        while self.current != '\n' {
            self.next();
        }
    }

    fn is_valid_identifier(&self, ch: char) -> bool {
        ch.is_alphanumeric() || ch == '_'
    }

    fn identifier(&mut self) -> Token {
        let start = self.loc();

        let mut value = String::new();
        value.push(self.current);

        self.next();
        while self.is_valid_identifier(self.current) {
            value.push(self.current);
            self.next();
        }

        let kind: TokenKind = match KEYWORDS.get(&value[..]) {
            Some(kind) => kind.clone(),
            None => TokenKind::Identifier(value)
        };

        Token { kind, span: self.make_span(start, self.loc()) }
    }

    fn number(&mut self) -> Token {
        let start = self.loc();

        let mut value = String::new();
        value.push(self.current);

        self.next();
        while self.current.is_numeric() {
            value.push(self.current);
            self.next();
        }

        Token { kind: TokenKind::Number(value), span: self.make_span(start, self.loc()) }
    }

    fn string(&mut self) -> Token {
        let quote = self.current;
        let start = self.loc();

        let mut value = String::new();
        self.next();

        while self.current != quote {
            value.push(self.current);
            self.next();
        }

        self.next();
        Token { kind: TokenKind::String(value), span: self.make_span(start, self.loc()) }
    }

    fn symbol(&mut self) -> Token {
        let start = self.loc();
        let current = self.current;

        self.next();
        match current {
            '+' => {
                let kind = match self.current {
                    '+' => { self.next(); TokenKind::Inc },
                    '=' => { self.next(); TokenKind::IAdd },
                    _ => TokenKind::Add
                };

                Token::new(kind, self.make_span(start, self.loc()))
            },
            '-' => {
                let kind = match self.current {
                    '-' => { self.next(); TokenKind::Dec },
                    '=' => { self.next(); TokenKind::IMinus },
                    '>' => { self.next(); TokenKind::Arrow },
                    _ => TokenKind::Minus
                };

                Token::new(kind, self.make_span(start, self.loc()))
            },
            '*' => {
                let kind = self.expect_or('=', TokenKind::IMul, TokenKind::Mul);
                Token::new(kind, self.make_span(start, self.loc()))
            },
            '/' => {
                let kind = self.expect_or('=', TokenKind::IDiv, TokenKind::Div);
                Token::new(kind, self.make_span(start, self.loc()))
            },
            '&' => {
                let kind = self.expect_or('&', TokenKind::And, TokenKind::BinaryAnd);
                Token::new(kind, self.make_span(start, self.loc()))
            },
            '|' => {
                let kind = self.expect_or('|', TokenKind::Or, TokenKind::BinaryOr);
                Token::new(kind, self.make_span(start, self.loc()))
            },
            '=' => {
                let kind = match self.current {
                    '=' => { self.next(); TokenKind::Eq },
                    '>' => { self.next(); TokenKind::DoubleArrow },
                    _ => TokenKind::Assign
                };

                Token::new(kind, self.make_span(start, self.loc()))
            },
            '<' => {
                let kind = match self.current {
                    '=' => { self.next(); TokenKind::Lte },
                    '<' => { self.next(); TokenKind::Shl },
                    _ => TokenKind::Lt
                };

                Token::new(kind, self.make_span(start, self.loc()))
            },
            '>' => {
                let kind = match self.current {
                    '=' => { self.next(); TokenKind::Gte },
                    '>' => { self.next(); TokenKind::Shr },
                    _ => TokenKind::Gt
                };

                Token::new(kind, self.make_span(start, self.loc()))
            },
            ':' => {
                let kind = self.expect_or(':', TokenKind::DoubleColon, TokenKind::Colon);
                Token::new(kind, self.make_span(start, self.loc()))
            },
            '!' => {
                let kind = self.expect_or('=', TokenKind::Neq, TokenKind::Not);
                Token::new(kind, self.make_span(start, self.loc()))
            },
            '%' => Token::new(TokenKind::Mod, self.make_span(start, self.loc())),
            '^' => Token::new(TokenKind::Xor, self.make_span(start, self.loc())),
            '(' => Token::new(TokenKind::LParen, self.make_span(start, self.loc())),
            ')' => Token::new(TokenKind::RParen, self.make_span(start, self.loc())),
            '{' => Token::new(TokenKind::LBrace, self.make_span(start, self.loc())),
            '}' => Token::new(TokenKind::RBrace, self.make_span(start, self.loc())),
            '[' => Token::new(TokenKind::LBracket, self.make_span(start, self.loc())),
            ']' => Token::new(TokenKind::RBracket, self.make_span(start, self.loc())),
            ',' => Token::new(TokenKind::Comma, self.make_span(start, self.loc())),
            '.' => Token::new(TokenKind::Dot, self.make_span(start, self.loc())),
            ';' => Token::new(TokenKind::SemiColon, self.make_span(start, self.loc())),
            '?' => Token::new(TokenKind::Maybe, self.make_span(start, self.loc())),
            '~' => Token::new(TokenKind::BinaryNot, self.make_span(start, self.loc())),
            _ => error!(&self.make_span(start, self.loc()), "Unexpected symbol: {}", current)
        }
    }

    pub fn once(&mut self) -> Token {
        match self.current {
            'a'..='z' | 'A'..='Z' | '_' => self.identifier(),
            '0'..='9' => self.number(),
            '"' => self.string(),
            '\'' => {
                let start = self.loc();
                self.next();

                let value = self.current;
                self.next();

                if self.current != '\'' {
                    error!(&self.make_span(self.loc(), self.loc()), "Expected closing quote");
                }

                Token { kind: TokenKind::Char(value), span: self.make_span(start, self.loc()) }
            }
            '#' => {
                self.skip_comment();

                self.next();
                self.once()
            }
            _ => self.symbol()
        }
    }

    pub fn lex(&mut self) -> Vec<Token> {
        let mut tokens = Vec::new();

        while !self.eof {
            self.skip_whitespace();
            tokens.push(self.once());
        }

        tokens.push(Token::new(TokenKind::EOF, self.make_span(self.loc(), self.loc())));
        tokens
    }
}
