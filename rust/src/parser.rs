use std::collections::{VecDeque, HashMap};

use crate::ast::*;
use crate::tokens::{Span, TokenKind, Token, span};
use crate::log::{error, note};

pub struct Parser<'a> {
    tokens: &'a Vec<Token>,
    index: usize,
    current: &'a Token,

    is_inside_function: bool,
    is_inside_loop: bool,
    is_inside_struct: bool,
    is_inside_impl: bool,
}

impl Parser<'_> {
    pub fn new(tokens: &'_ Vec<Token>) -> Parser {
        Parser {
            tokens,
            index: 1,
            current: &tokens[0],

            is_inside_function: false,
            is_inside_loop: false,
            is_inside_struct: false,
            is_inside_impl: false,
        }
    }

    fn next(&mut self) {
        self.current = &self.tokens[self.index];
        self.index += 1;
    }

    fn peek(&self, n: usize) -> &Token {
        &self.tokens[self.index + n - 1]
    }

    fn expect(&mut self, kind: TokenKind, token: &str) {
        if self.current.kind != kind {
            error!(&self.current.span.clone(), "Expected {}", token);
        }

        self.next();
    }

    fn parse_path(&mut self, name: Option<String>) -> Path {
        let mut path = Path {
            name: name.unwrap_or_else(|| {
                if let TokenKind::Identifier(ref name) = self.current.kind {
                    self.next();
                    name.to_string()
                } else {
                    error!(&self.current.span.clone(), "Expected identifier");
                }
            }),
            segments: VecDeque::new(),
        };

        while self.current.kind == TokenKind::DoubleColon {
            self.next();

            if let TokenKind::Identifier(ref name) = self.current.kind {
                path.segments.push_back(name.to_string());
                self.next();
            } else {
                error!(&self.current.span.clone(), "Expected identifier");
            }
        }

        path
    }

    fn parse_identifier(&mut self) -> (String, Span) {
        if let TokenKind::Identifier(ref name) = self.current.kind {
            let span = self.current.span.clone();
            self.next();

            (name.to_string(), span)
        } else {
            error!(&self.current.span.clone(), "Expected identifier");
        }
    }

    fn parse_type(&mut self) -> Box<Type> {
        let start = self.current.span.clone();
        match self.current.kind {
            TokenKind::Identifier(ref value) => {
                self.next();

                let path = self.parse_path(Some(value.to_string()));
                Type::new(span!(start, self.current.span), TypeKind::Named(path))
            },
            TokenKind::LParen => {
                self.next();

                let mut types = Vec::new();
                while self.current.kind != TokenKind::RParen {
                    types.push(self.parse_type());

                    if self.current.kind != TokenKind::Comma {
                        break;
                    }

                    self.next();
                }

                if self.current.kind != TokenKind::RParen {
                    error!(&self.current.span.clone(), "Expected ')'");
                }

                self.next();

                Type::new(span!(start, self.current.span), TypeKind::Tuple(types))
            },
            TokenKind::LBracket => {
                self.next();

                let ty = self.parse_type();
                self.expect(TokenKind::SemiColon, "';'");

                let size = self.expr(false);    
                self.expect(TokenKind::RBracket, "']'");

                self.next();
                Type::new(span!(start, self.current.span), TypeKind::Array(ty, size))
            },
            TokenKind::BinaryAnd => {
                let mut is_mutable = false;
                self.next();

                if self.current.kind == TokenKind::Mut {
                    self.next();
                    is_mutable = true;
                }

                Type::new(span!(start, self.current.span), TypeKind::Ref(self.parse_type(), is_mutable))
            },
            TokenKind::Mul => {
                let mut is_mutable = false;
                self.next();

                if self.current.kind == TokenKind::Mut {
                    self.next();
                    is_mutable = true;
                }

                let ty = self.parse_type();
                if let TypeKind::Ref(_, _) = ty.kind {
                    error!(&self.current.span.clone(), "Cannot have a pointer to a reference");
                }

                Type::new(span!(start, self.current.span), TypeKind::Ptr(ty, is_mutable))
            }
            TokenKind::Func => {
                self.next();
                self.expect(TokenKind::LParen, "'('");

                let mut args = Vec::new();
                while self.current.kind != TokenKind::RParen {
                    args.push(self.parse_type());
                }

                self.expect(TokenKind::RParen, "')'");

                let mut ret = None;
                if self.current.kind == TokenKind::Arrow {
                    self.next();
                    ret = Some(self.parse_type());
                }

                Type::new(span!(start, self.current.span), TypeKind::Func(args, ret))
            }
            _ => error!(&self.current.span.clone(), "Expected type")
        }
    }

    fn parse_type_alias(&mut self) -> Box<Expr> {
        let start = self.current.span.clone();
        self.next();
        
        let name = if let TokenKind::Identifier(ref name) = self.current.kind {
            self.next();
            name.to_string()
        } else {
            error!(&self.current.span.clone(), "Expected identifier");
        };

        self.expect(TokenKind::Eq, "'='");
        let ty = self.parse_type();

        Expr::new(span!(start, self.current.span), ExprKind::Type(name, ty))
    }

    fn parse_variable_definition(&mut self, is_const: bool) -> Box<Expr> {
        let start = self.current.span.clone();
        self.next();

        let mut idents: Vec<Identifier> = Vec::new();
        let mut ty: Option<Box<Type>> = None;
        let mut value: Option<Box<Expr>> = None;
        let consume: Option<String> = None;
        let mut is_tuple_unpack = false;

        let mut is_mutable = false;
        if self.current.kind == TokenKind::Mut {
            is_mutable = true;
            self.next();
        }

        if let TokenKind::Identifier(ref name) = self.current.kind {
            idents.push(Identifier {
                value: name.to_string(),
                span: self.current.span.clone(),
                is_mutable
            });
            self.next();
        } else if self.current.kind == TokenKind::LParen {
            self.next();
            is_tuple_unpack = true;

            while self.current.kind != TokenKind::RParen {
                let mut is_local_mutable = false;
                if self.current.kind == TokenKind::Mut {
                    if is_mutable {
                        note!(&self.current.span.clone(), "Redundant 'mut'");
                    }

                    is_local_mutable = true;
                    self.next();
                }

                if let TokenKind::Identifier(ref name) = self.current.kind {
                    idents.push(Identifier {
                        value: name.to_string(),
                        span: self.current.span.clone(),
                        is_mutable: is_mutable || is_local_mutable
                    });
                    self.next();
                } else {
                    error!(&self.current.span.clone(), "Expected identifier");
                }

                if self.current.kind != TokenKind::Comma {
                    break;
                }

                self.next();
            }

            self.expect(TokenKind::RParen, "')'");
        } else {
            error!(&self.current.span.clone(), "Expected an identifier or '('");
        }

        if self.current.kind == TokenKind::Colon {
            self.next();
            ty = Some(self.parse_type());
        }

        if self.current.kind == TokenKind::Assign {
            self.next();
            value = Some(self.expr(false));
        }

        self.expect(TokenKind::SemiColon, "';'");

        let expr = Box::new(VarAssignExpr {
            idents, ty, value, consume, is_tuple_unpack
        });

        if is_const {
            Expr::new(span!(start, self.current.span), ExprKind::Const(expr))
        } else {
            Expr::new(span!(start, self.current.span), ExprKind::Let(expr))
        }
    }

    fn parse_function_arguments(&mut self) -> Vec<Argument> {
        let mut args = Vec::new();

        let mut has_kwargs = false;
        let mut has_default_values = false;
        let mut _is_variadic = false;
        let mut _is_c_variadic = false;

        while self.current.kind != TokenKind::RParen {
            let mut is_mutable = false;
            let mut is_self = false;
            let mut span = self.current.span.clone();

            if self.current.kind == TokenKind::Mut {
                is_mutable = true;
                self.next();
            } 
        
            let name: String;
            if self.current.kind == TokenKind::Mul {
                self.next();
                self.expect(TokenKind::Comma, "','");

                has_kwargs = true;
                continue;
            } else if let TokenKind::Identifier(ref ident) = self.current.kind {
                name = ident.to_string();
                span = span!(span, self.current.span);

                self.next();
            } else {
                error!(&self.current.span.clone(), "Expected an identifier or '*'");
            }

            let mut ty = None;
            if name != "self" {
                self.expect(TokenKind::Colon, "':'");
                ty = Some(self.parse_type());
            } else if self.is_inside_impl || self.is_inside_struct {
                is_self = true;
            } else {
                self.expect(TokenKind::Colon, "':'");
                ty = Some(self.parse_type());
            }

            let mut default_value = None;
            if self.current.kind == TokenKind::Assign {
                self.next();

                default_value = Some(self.expr(false));
                has_default_values = true;
            }

            if has_default_values && default_value.is_none() {
                error!(&self.current.span.clone(), "Expected default value");
            }

            if self.current.kind == TokenKind::Comma {
                self.next();
            }

            args.push(Argument {
                name,
                ty,
                default: default_value,
                is_self,
                is_kwarg: has_kwargs,
                is_mutable,
                is_variadic: false,
                span
            });
        }

        args
    }

    fn parse_function_prototype(&mut self, linkage: Linkage) -> Box<PrototypeExpr> {
        let name: String;
        if let TokenKind::Identifier(ref ident) = self.current.kind {
            name = ident.to_string();
            self.next();
        } else {
            error!(&self.current.span.clone(), "Expected an identifier");
        }

        self.expect(TokenKind::LParen, "'('");
        let args = self.parse_function_arguments();

        self.expect(TokenKind::RParen, "')'");
        let mut ret = None;

        if self.current.kind == TokenKind::Arrow {
            self.next();
            ret = Some(self.parse_type());
        }

        Box::new(PrototypeExpr {
            name, args, ret, is_c_variadic: false, linkage
        })
    }

    fn parse_function(&mut self, linkage: Linkage) -> Box<Expr> {
        let start = self.current.span.clone();
        self.next();

        let prototype = self.parse_function_prototype(linkage);
        if self.current.kind == TokenKind::SemiColon {
            let span = span!(start, self.current.span);
            self.next();

            return Expr::new(span, ExprKind::Prototype(prototype));
        }

        let outer = self.is_inside_function;
        self.is_inside_function = true;

        self.expect(TokenKind::LBrace, "'{'");
        let body = self.parse_block();

        self.is_inside_function = outer;
        Expr::new(span!(start, self.current.span), ExprKind::Func(prototype, body))
    }

    fn parse_function_call(&mut self, callee: Box<Expr>) -> Box<Expr> {
        let mut args = Vec::new();
        let mut kwargs = HashMap::new();

        let mut has_kwargs = false;
        while self.current.kind != TokenKind::RParen {
            if let TokenKind::Identifier(ref ident) = self.current.kind {
                if self.peek(1).kind != TokenKind::Assign {
                    args.push(self.expr(false));

                    if self.current.kind != TokenKind::Comma {
                        break;
                    }

                    self.next();
                    continue;
                }

                if kwargs.contains_key(ident) {
                    error!(&self.current.span.clone(), "Duplicate keyword argument '{}'", ident);
                }

                self.next(); self.next();
                kwargs.insert(ident.to_string(), self.expr(false));

                has_kwargs = true;
            } else {
                if has_kwargs {
                    error!(&self.current.span.clone(), "Expected an identifier");
                }

                args.push(self.expr(false));
            }

            if self.current.kind != TokenKind::Comma {
                break;
            }

            self.next();
        }

        self.expect(TokenKind::RParen, "')'");
        Expr::new(span!(callee.span, self.current.span), ExprKind::Call(callee, args, kwargs))   
    }

    fn parse_struct(&mut self) -> Box<Expr> {
        self.next();
        let (name, span) = if let TokenKind::Identifier(ref ident) = self.current.kind {
            (ident.to_string(), self.current.span.clone())
        } else {
            error!(&self.current.span.clone(), "Expected an identifier");
        };

        self.next();

        let mut fields = Vec::new();
        let mut body = Vec::new();
        let mut parents = Vec::new();
        let mut index: usize = 0;

        while self.current.kind == TokenKind::LParen {
            parents.push(self.parse_path(None));
            if self.current.kind != TokenKind::Comma {
                self.expect(TokenKind::RParen, "')'");
                break;
            }

            self.next();
        }

        self.expect(TokenKind::LBrace, "'{'");
        self.is_inside_struct = true;

        while self.current.kind != TokenKind::RBrace {
            let mut is_private = false;
            let mut is_readonly = false;
            
            match self.current.kind {
                TokenKind::Private => { is_private = true; self.next(); }
                TokenKind::Readonly => { is_readonly = true; self.next(); }
                _ => {}
            }

            match self.current.kind {
                TokenKind::Identifier(ref ident) => {
                    let name = ident.to_string();
                    self.next();

                    self.expect(TokenKind::Colon, "':'");
                    let ty = self.parse_type();

                    fields.push(StructField {
                        name,
                        ty,
                        index,
                        is_private,
                        is_readonly
                    });

                    index += 1;
                    self.expect(TokenKind::SemiColon, "';'");
                }
                TokenKind::Const => body.push(self.parse_variable_definition(true)),
                TokenKind::Type => body.push(self.parse_type_alias()),
                TokenKind::Func => body.push(self.parse_function(Linkage::None)),
                _ => error!(&self.current.span.clone(), "Expected an identifier, function definition, or type alias")
            }
        }

        self.expect(TokenKind::RBrace, "'}'");
        let expr = Box::new(StructExpr {
            name,
            opaque: false,
            fields,
            body,
            parents
        });

        self.is_inside_struct = false;
        Expr::new(span, ExprKind::Struct(expr))
    }

    fn parse_struct_literal(&mut self, structure: Box<Expr>) -> Box<Expr> {
        let mut fields = HashMap::new();
        while self.current.kind != TokenKind::RBrace {
            if let TokenKind::Identifier(ref ident) = self.current.kind {
                if fields.contains_key(ident) {
                    error!(&self.current.span.clone(), "Duplicate field '{}'", ident);
                }

                self.next();

                self.expect(TokenKind::Colon, "':'");
                fields.insert(ident.to_string(), self.expr(false));
            } else {
                error!(&self.current.span.clone(), "Expected an identifier");
            }

            if self.current.kind != TokenKind::Comma {
                break;
            }

            self.next();
        }

        self.expect(TokenKind::RBrace, "'}'");
        Expr::new(span!(self.current.span, self.current.span), ExprKind::StructLiteral(structure, fields))
    }

    fn parse_impl(&mut self) -> Box<Expr> {
        self.next();

        let ty = self.parse_type();
        self.expect(TokenKind::LBrace, "'{'");

        let outer = self.is_inside_impl;
        self.is_inside_impl = true;

        let mut body = Vec::new();  
        while self.current.kind != TokenKind::RBrace {
            match self.current.kind {
                TokenKind::Const => body.push(self.parse_variable_definition(true)),
                TokenKind::Type => body.push(self.parse_type_alias()),
                TokenKind::Func => body.push(self.parse_function(Linkage::None)),
                _ => error!(&self.current.span.clone(), "Expected an identifier, function definition, or type alias")
            }
        }

        self.is_inside_impl = outer;

        self.expect(TokenKind::RBrace, "'}'");
        Expr::new(ty.span.clone(), ExprKind::Impl(ty, body))
    }

    fn parse_enum(&mut self) -> Box<Expr> {
        self.next();
        let (name, span) = self.parse_identifier();

        self.expect(TokenKind::LBrace, "'{'");
        let mut enumerators = Vec::new();

        while self.current.kind != TokenKind::RBrace {
            let (field, _) = self.parse_identifier();
            let mut value = None;

            if self.current.kind == TokenKind::Assign {
                self.next();
                value = Some(self.expr(false));
            }
            
            enumerators.push(Enumerator { name: field, value });

            if self.current.kind != TokenKind::Comma {
                break;
            }

            self.next();
        }

        self.expect(TokenKind::RBrace, "'}'");
        Expr::new(span, ExprKind::Enum(name, enumerators))
    }

    fn parse_block(&mut self) -> Vec<Box<Expr>> {
        let mut block = Vec::new();
        while self.current.kind != TokenKind::RBrace {
            block.push(self.stmt());
        }

        self.expect(TokenKind::RBrace, "'}'");
        block
    }

    pub fn parse(&mut self) -> Vec<Box<Expr>> {
        let mut stmts = Vec::new();
        while self.current.kind != TokenKind::EOF {
            stmts.push(self.stmt());
        }

        stmts
    }

    fn stmt(&mut self) -> Box<Expr> {
        match self.current.kind {
            TokenKind::Import => {
                self.next();
                let path = self.parse_path(None);

                let mut items: Vec<String> = Vec::new();
                let mut alias = None;
                let mut is_wildcard = false;

                match self.current.kind {
                    TokenKind::As => {
                        self.next();
                        if let TokenKind::Identifier(ref name) = self.current.kind {
                            alias = Some(name.to_string());
                            self.next();
                        } else {
                            error!(&self.current.span.clone(), "Expected identifier");
                        }
                    },
                    TokenKind::LBrace => {
                        self.next();

                        while self.current.kind != TokenKind::RBrace {
                            if let TokenKind::Identifier(ref name) = self.current.kind {
                                items.push(name.to_string());
                                self.next();
                            } else {
                                error!(&self.current.span.clone(), "Expected identifier");
                            }

                            if self.current.kind != TokenKind::Comma {
                                break;
                            }

                            self.next();
                        }

                        self.expect(TokenKind::RBrace, "'}'");
                    },
                    TokenKind::Mul => {
                        self.next();
                        is_wildcard = true;
                    }
                    _ => {}
                };

                self.expect(TokenKind::SemiColon, "';'");
                Expr::new(
                    span!(self.current.span, self.current.span), 
                    ExprKind::Import(path, alias, items, is_wildcard)
                )
            },
            TokenKind::While => {
                self.next();
                let cond = self.expr(false);

                let outer = self.is_inside_loop;
                self.is_inside_loop = true;

                self.expect(TokenKind::LBrace, "'{'");

                let body = self.parse_block();
                self.is_inside_loop = outer;

                Expr::new(
                    span!(self.current.span, self.current.span), 
                    ExprKind::While(cond, body)
                )
            },
            TokenKind::If => {
                self.next();
                let cond = self.expr(false);

                self.expect(TokenKind::LBrace, "'{'");
                let body = self.parse_block();

                let mut else_body: Option<ExprList> = None;
                if self.current.kind == TokenKind::Else {
                    self.next();

                    if self.current.kind == TokenKind::If {
                        else_body = Some(vec![self.stmt()]);
                    } else {
                        self.expect(TokenKind::LBrace, "'{'");
                        else_body = Some(self.parse_block());
                    }
                }

                Expr::new(
                    span!(self.current.span.clone(), self.current.span), 
                    ExprKind::If(cond, body, else_body)
                )
            },
            TokenKind::Return => {
                self.next();
                if !self.is_inside_function {
                    error!(&self.current.span, "Cannot return outside of a function");
                }

                if self.current.kind == TokenKind::SemiColon {
                    return Expr::new(
                        span!(self.current.span, self.current.span), 
                        ExprKind::Ret(None)
                    );
                }

                let expr = self.expr(true);
                Expr::new(
                    span!(self.current.span, self.current.span), 
                    ExprKind::Ret(Some(expr))
                )
            },
            TokenKind::Break => {
                if !self.is_inside_loop {
                    error!(&self.current.span, "Cannot break outside of a loop");
                }

                self.next();
                self.expect(TokenKind::SemiColon, "';'");

                Expr::new(span!(self.current.span, self.current.span), ExprKind::Break)
            },
            TokenKind::Continue => {
                if !self.is_inside_loop {
                    error!(&self.current.span, "Cannot continue outside of a loop");
                }

                self.next();
                self.expect(TokenKind::SemiColon, "';'");

                Expr::new(span!(self.current.span, self.current.span), ExprKind::Continue)
            },
            TokenKind::Struct => self.parse_struct(),
            TokenKind::Impl => self.parse_impl(),
            TokenKind::Enum => self.parse_enum(),
            TokenKind::Let => self.parse_variable_definition(false),
            TokenKind::Const => self.parse_variable_definition(true),
            TokenKind::Func => self.parse_function(Linkage::None),
            _ => self.expr(true),
        }
    }

    fn expr(&mut self, end: bool) -> Box<Expr> {
        let mut expr = self.unary();
        expr = self.binary(expr, 0);

        if end {
            self.expect(TokenKind::SemiColon, "';'");
        }

        expr
    }

    fn binary(&mut self, mut left: Box<Expr>, precedence: i8) -> Box<Expr> {
        loop {
            if self.current.kind.precedence() < precedence {
                return left;
            }

            let operation = &self.current.kind;
            self.next();

            let mut right = self.unary();
            if operation.precedence() < self.current.kind.precedence() {
                right = self.binary(right, operation.precedence() + 1);
            }

            let op = match operation {
                TokenKind::Add | TokenKind::IAdd => BinaryOp::Add,
                TokenKind::Minus | TokenKind::IMinus => BinaryOp::Sub,
                TokenKind::Mul | TokenKind::IMul => BinaryOp::Mul,
                TokenKind::Div | TokenKind::IDiv => BinaryOp::Div,
                TokenKind::Mod => BinaryOp::Mod,
                TokenKind::Eq => BinaryOp::Eq,
                TokenKind::Neq => BinaryOp::Neq,
                TokenKind::Lt => BinaryOp::Lt,
                TokenKind::Lte => BinaryOp::Lte,
                TokenKind::Gt => BinaryOp::Gt,
                TokenKind::Gte => BinaryOp::Gte,
                TokenKind::And => BinaryOp::And,
                TokenKind::Or => BinaryOp::Or,
                TokenKind::Shl => BinaryOp::Shl,
                TokenKind::Shr => BinaryOp::Shr,
                TokenKind::Xor => BinaryOp::Xor,
                TokenKind::BinaryAnd => BinaryOp::BinaryAnd,
                TokenKind::BinaryOr => BinaryOp::BinaryOr,
                TokenKind::Assign => BinaryOp::Assign,
                _ => unreachable!()
            };

            if operation.is_inplace_binary_op() {
                left = Expr::new(span!(left.span, right.span), ExprKind::InplaceBinOp(op, left, right));
            } else {
                left = Expr::new(span!(left.span, right.span), ExprKind::BinaryOp(op, left, right));
            }
        }
    }

    fn unary(&mut self) -> Box<Expr> {
        let op = match self.current.kind {
            TokenKind::Minus => UnaryOp::Neg,
            TokenKind::Not => UnaryOp::Not,
            TokenKind::BinaryNot => UnaryOp::BinaryNot,
            TokenKind::BinaryAnd => UnaryOp::Ref,
            TokenKind::Mul => UnaryOp::Deref,
            TokenKind::Inc => UnaryOp::Inc,
            TokenKind::Dec => UnaryOp::Dec,
            _ => return self.call()
        };

        let start = self.current.span.clone();
        self.next();

        let expr = self.unary();
        Expr::new(span!(start, expr.span), ExprKind::UnaryOp(op, expr))
    }

    fn call(&mut self) -> Box<Expr> {
        let mut expr = self.primary();

        if self.current.kind == TokenKind::LParen {
            self.next();
            expr = self.parse_function_call(expr);
        } else if self.current.kind == TokenKind::LBrace && self.peek(2).kind == TokenKind::Colon {
            self.next();
            expr = self.parse_struct_literal(expr);
        }

        if self.current.kind == TokenKind::Dot {
            expr = self.attr(expr);
        } else if self.current.kind == TokenKind::LBracket {
            expr = self.element(expr);
        }

        match self.current.kind {
            TokenKind::As => {
                self.next();
                let ty = self.parse_type();

                Expr::new(span!(expr.span, ty.span), ExprKind::Cast(expr, ty))
            }
            TokenKind::If => {
                self.next();

                let cond = self.expr(false);
                self.expect(TokenKind::Else, "else");

                let else_expr = self.expr(false);

                Expr::new(span!(expr.span, else_expr.span), ExprKind::Ternary(expr, cond, else_expr))
            }
            _ => expr,
        }
    }

    fn element(&mut self, mut expr: Box<Expr>) -> Box<Expr> {
        while self.current.kind == TokenKind::LBracket {
            self.next();

            let index = self.expr(false);
            expr = Expr::new(span!(expr.span, index.span), ExprKind::Index(expr, index));

            self.expect(TokenKind::RBracket, "']'");
        }

        if self.current.kind == TokenKind::Dot {
            self.attr(expr)
        } else {
            expr
        }
    }

    fn attr(&mut self, mut expr: Box<Expr>) -> Box<Expr> {
        while self.current.kind == TokenKind::Dot {
            self.next();

            let name = match self.current.kind {
                TokenKind::Identifier(ref name) => name.to_string(),
                _ => error!(&self.current.span.clone(), "Expected an identifier")
            };

            self.next();
            expr = Expr::new(span!(expr.span, self.current.span), ExprKind::Attr(expr, name));
        }

        if self.current.kind == TokenKind::LBracket {
            self.element(expr)
        } else {
            expr
        }
    }

    fn primary(&mut self) -> Box<Expr> {
        let expr = match self.current.kind {
            TokenKind::Number(ref value) => {
                let span = self.current.span.clone();
                self.next();

                Expr::new(span, ExprKind::Integer(value.to_string()))
            },
            TokenKind::String(ref value) => {
                let span = self.current.span.clone();
                self.next();

                Expr::new(span, ExprKind::String(value.to_string()))
            },
            TokenKind::Identifier(ref value) => {
                let span = self.current.span.clone();
                self.next();

                if self.current.kind == TokenKind::DoubleColon {
                    Expr::new(
                        span!(span, self.current.span),
                        ExprKind::Path(self.parse_path(Some(value.to_string())))
                    )
                } else {
                    Expr::new(span, ExprKind::Identifier(value.to_string()))
                }
            },
            TokenKind::LParen => {
                self.next();
                let expr = self.expr(false);

                if self.current.kind == TokenKind::Comma {
                    self.next();
                    let mut items = vec![expr];

                    while self.current.kind != TokenKind::RParen {
                        items.push(self.expr(false));
                        if self.current.kind != TokenKind::Comma {
                            break;
                        }

                        self.next();
                    }

                    let end = self.current.span.clone();
                    self.expect(TokenKind::RParen, "')'");

                    return Expr::new(span!(items[0].span, end), ExprKind::Tuple(items));
                }

                self.expect(TokenKind::RParen, "')'");
                expr
            },
            TokenKind::LBracket => {
                let start = self.current.span.clone();
                self.next();

                let mut elements = Vec::new();
                while self.current.kind != TokenKind::RBracket {
                    elements.push(self.expr(false));

                    if self.current.kind != TokenKind::Comma {
                        break;
                    }

                    self.next();
                }

                let end = self.current.span.clone();
                if self.current.kind != TokenKind::RBracket {
                    error!(&self.current.span.clone(), "Expected ']'");
                }

                self.next();
                Expr::new(span!(start, end), ExprKind::Array(elements))  
            },
            TokenKind::Sizeof => {
                self.next();
                self.expect(TokenKind::LParen, "'('");

                let ty = self.parse_type();
                self.expect(TokenKind::RParen, "')'");

                Expr::new(span!(self.current.span, ty.span), ExprKind::Sizeof(ty))
            },
            _ => {
                error!(&self.current.span.clone(), "Expected an expression");
            }
        };

        match self.current.kind {
            TokenKind::Dot => self.attr(expr),
            TokenKind::LBracket => self.element(expr),
            _ => expr
        }
    }

}