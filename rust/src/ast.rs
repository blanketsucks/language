use crate::tokens::Span;

use std::{boxed::Box, collections::{VecDeque, HashMap}};

pub type ExprList = Vec<Box<Expr>>;

#[derive(Debug)]
pub struct Path {
    pub name: String,
    pub segments: VecDeque<String>
}

#[derive(Debug)]
pub struct Argument {
    pub name: String,

    pub ty: Option<Box<Type>>,
    pub default: Option<Box<Expr>>,

    pub is_self: bool,
    pub is_kwarg: bool,
    pub is_mutable: bool,
    pub is_variadic: bool,

    pub span: Span
}

#[derive(Debug)]
pub struct StructField {
    pub name: String,
    pub ty: Box<Type>,
    pub index: usize,
    pub is_readonly: bool,
    pub is_private: bool
}

#[derive(Debug)]
pub struct Identifier {
    pub value: String,
    pub span: Span,
    pub is_mutable: bool
}

#[derive(Debug)]
pub struct VarAssignExpr {
    pub idents: Vec<Identifier>,
    pub ty: Option<Box<Type>>,
    pub value: Option<Box<Expr>>,
    pub consume: Option<String>,
    pub is_tuple_unpack: bool
}

#[derive(Debug)]
pub struct CallExpr {
    pub callee: Box<Expr>,
    pub args: ExprList,
    pub kwargs: HashMap<String, Box<Expr>>
}

#[derive(Debug)]
pub struct PrototypeExpr {
    pub name: String,
    pub args: Vec<Argument>,
    pub ret: Option<Box<Type>>,
    pub is_c_variadic: bool,
    pub linkage: Linkage
}

#[derive(Debug)]
pub struct StructExpr {
    pub name: String,
    pub opaque: bool,
    pub fields: Vec<StructField>,
    pub parents: Vec<Path>,
    pub body: ExprList
}

#[derive(Debug)]
pub struct Enumerator {
    pub name: String,
    pub value: Option<Box<Expr>>
}

#[derive(Debug)]
pub enum Linkage {
    None,
    Unspecified,
    C
}

#[derive(Debug)]
pub enum UnaryOp {
    Neg,
    Not,
    BinaryNot,
    Deref,
    Ref,
    Inc,
    Dec
}

#[derive(Debug)]
pub enum BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    And,
    Or,
    Xor,
    Shl,
    Shr,
    BinaryAnd,
    BinaryOr,
    Eq,
    Neq,
    Lt,
    Gt,
    Lte,
    Gte,
    Assign
}


#[derive(Debug)]
pub enum ExprKind {
    Block(ExprList),

    String(String),
    Integer(String),
    Float(String),

    Array(ExprList),
    Index(Box<Expr>, Box<Expr>),
    Tuple(ExprList),

    Identifier(String),
    Let(Box<VarAssignExpr>),
    Const(Box<VarAssignExpr>),

    UnaryOp(UnaryOp, Box<Expr>),
    BinaryOp(BinaryOp, Box<Expr>, Box<Expr>),
    InplaceBinOp(BinaryOp, Box<Expr>, Box<Expr>),

    Cast(Box<Expr>, Box<Type>),
    Type(String, Box<Type>),
    Sizeof(Box<Type>), // TODO: sizeof expr

    Call(Box<Expr>, ExprList, HashMap<String, Box<Expr>>),
    Ret(Option<Box<Expr>>),

    Prototype(Box<PrototypeExpr>),
    Func(Box<PrototypeExpr>, ExprList),

    If(Box<Expr>, ExprList, Option<ExprList>),
    While(Box<Expr>, ExprList),
    For(Vec<Identifier>, Box<Expr>, ExprList),
    Ternary(Box<Expr>, Box<Expr>, Box<Expr>),

    Break,
    Continue,

    Struct(Box<StructExpr>),
    Impl(Box<Type>, ExprList),
    StructLiteral(Box<Expr>, HashMap<String, Box<Expr>>),
    Attr(Box<Expr>, String),
    Path(Path),

    Enum(String, Vec<Enumerator>),

    Import(Path, Option<String>, Vec<String>, bool) // path, alias, items, is_wildcard
}

#[derive(Debug)]
pub struct Expr {
    pub kind: ExprKind,
    pub span: Span
}

impl Expr {
    pub fn new(span: Span, kind: ExprKind) -> Box<Self> {
        Box::new(Self { kind, span })
    }
}

#[derive(Debug)]
pub enum TypeKind {
    Named(Path),
    Tuple(Vec<Box<Type>>),
    Array(Box<Type>, Box<Expr>),
    Func(Vec<Box<Type>>, Option<Box<Type>>),
    Ptr(Box<Type>, bool),
    Ref(Box<Type>, bool)
}

#[derive(Debug)]
pub struct Type {
    pub kind: TypeKind,
    pub span: Span
}

impl Type {
    pub fn new(span: Span, kind: TypeKind) -> Box<Self> {
        Box::new(Self { kind, span })
    }
}