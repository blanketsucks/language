use std::process::Command;

use crate::llvm;
use crate::lexer;
use crate::parser;
use crate::visitor;
use crate::log::error;

pub enum OutputFormat {
    Object,
    Executable,
    Assembly,
    LLVM,
    Bitcode,
    Library
}

pub enum OptimizationLevel {
    Debug,
    Release,
}

pub struct Compiler {
    input: String,
    output: String,
    format: OutputFormat,
    target: Option<String>
}

impl Compiler {
    pub fn new(input: String, output: String, format: OutputFormat) -> Self {
        Self { input, output, format, target: None }
    }

    pub fn with_target(mut self, target: String) -> Self {
        self.target = Some(target);
        self
    }

    pub fn compile(self) {
        let source = std::fs::read_to_string(
            &self.input
        ).unwrap_or_else(|err| error!("Failed to read file: {}", err));

        let mut lexer = lexer::Lexer::new(&self.input, &source);
        let tokens = lexer.lex();

        let mut parser = parser::Parser::new(&tokens);
        let ast = parser.parse();

        let mut visitor = visitor::Visitor::new(&self.input);
        visitor.visit(ast);

        let triple: String;
        if let Some(t) = self.target {
            triple = t;
        } else {
            triple = llvm::default_target_triple().to_string();
        }

        let target = llvm::Target::from_triple(&triple).unwrap_or_else(|err| {
            error!("Failed to get target: {}", err);
        });

        let machine = target.create_target_machine(
            &triple, llvm::default_cpu(), llvm::default_cpu_features(),
            llvm::CodeGenOptLevel::Default, llvm::RelocMode::Default,
            llvm::CodeModel::Default
        );

        visitor.module.set_data_layout(machine.create_data_layout());

        let mut path = std::path::PathBuf::from(&self.output);
        let filetype = match self.format {
            OutputFormat::Assembly => llvm::CodeGenFileType::AssemblyFile,
            OutputFormat::Executable | OutputFormat::Library => {
                let p = std::path::PathBuf::from(&self.input);
                path = p.with_extension("o");

                llvm::CodeGenFileType::ObjectFile
            }
            _ => llvm::CodeGenFileType::ObjectFile
        };

        machine.emit_to_file(
            &visitor.module, 
            path.to_str().unwrap(),
            filetype
        ).unwrap_or_else(|err| {
            error!("Failed to emit output file: {}", err);
        });

        if !matches!(self.format, OutputFormat::Executable | OutputFormat::Library) {
            return;
        }

        let mut cc = Command::new("cc");

        cc.arg(path.to_str().unwrap());
        cc.arg("-o").arg(&self.output);

        cc.output().unwrap_or_else(|err| {
            error!("Failed to execute cc: {}", err);
        });
    }
}