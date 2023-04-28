
use crate::llvm;
use crate::ast::Expr;

pub struct Visitor {
    pub context: llvm::Context,
    pub module: llvm::Module,
    pub builder: llvm::Builder
}

impl Visitor {
    pub fn new(filename: &str) -> Self {
        let context = llvm::Context::new();
        let module = llvm::Module::new(filename, Some(&context));
        let builder = llvm::Builder::new(Some(&context));

        Self { context, module, builder }
    }

    pub fn visit(&mut self, ast: Vec<Box<Expr>>) {
        for expr in ast {
            self.visit_expr(expr);
        }
    }

    fn visit_expr(&mut self, _expr: Box<Expr>) -> llvm::Value {
        unimplemented!()
    }
}