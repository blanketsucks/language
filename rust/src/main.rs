#![feature(extern_types)]

pub mod lexer;
pub mod tokens;
pub mod log;
pub mod ast;
pub mod parser;
pub mod llvm;
pub mod visitor;
pub mod compiler;

use std::time;

use lexer::Lexer;
use parser::Parser;
use log::error;

fn time<T>(func: impl FnOnce() -> T) -> (T, time::Duration) {
    let start = time::Instant::now();
    let ret = func();
    (ret, time::Instant::now() - start)
}

fn main() {
    let mut argv = std::env::args();
    let filename = argv.nth(1).unwrap_or_else(|| {
        error!("No input file specified");
    });

    let source = std::fs::read_to_string(&filename).unwrap_or_else(|err| {
        error!("Failed to read file '{}': {}", filename, err);
    });

    let mut lexer = Lexer::new(&filename, &source);
    let (tokens, duration) = time(|| lexer.lex());
    
    println!("Lexed {} tokens in {:?}", tokens.len(), duration);

    let mut parser = Parser::new(&tokens);
    let (ast, duration) = time(|| parser.parse());

    println!("Parsed {} nodes in {:?}", ast.len(), duration);
}
