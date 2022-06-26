#define _DEBUG_MODULE 0

#include "src/lexer.h"
#include "src/parser.h"
#include "src/types.h"
#include "src/visitor.h"

struct Arguments {
    std::string filename;
    std::string output;
    std::string entry;

    bool emit_llvm = false;
    bool emit_assembly = false;

    void dump() {
        std::cout << "Filename: " << this->filename << '\n';
        std::cout << "Output: " << this->output << '\n';
        std::cout << "Entry: " << this->entry << '\n';
        std::cout << "Emit LLVM: " << this->emit_llvm << '\n';
        std::cout << "Emit Assembly: " << this->emit_assembly << '\n';
        std::cout << std::endl;
    }
};

std::string replace_extension(std::string filename, std::string extension) {
    return filename.substr(0, filename.find_last_of('.')) + "." + extension;
}

[[noreturn]] void error(const std::string& message) {
    std::cerr << "\u001b[1;31m" << "error: " << "\u001b[0m" << message << std::endl;
    exit(1);
}

#ifdef __GNUC__

#include <getopt.h>

struct option* get_options() {
    struct option* options = new option[4];

    options[0] = {"output", required_argument, NULL, 'o'};
    options[1] = {"entry", required_argument, NULL, 'e'};
    options[2] = {"emit-llvm", no_argument, NULL, 'l'};
    options[3] = {"emit-assembly", no_argument, NULL, 'S'};

    return options;
}

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;

    option* options = get_options();
    while (true) {
        int index = 0;
        int c = getopt_long(argc, argv, "olS::", options, &index);

        if (c == -1) {
            break;
        }

        switch (c) {
            case 'o':
                args.output = optarg;
                break;
            case 'e':
                args.entry = optarg;
                break;
            case 'l':
                args.emit_llvm = true;
                break;
            case 'S':
                args.emit_assembly = true;
                break;
            case '?':
                break;
            default:
                exit(1);
        }
    }

    if (optind < argc) {
        args.filename = argv[optind];
    } else {
        error("No input file specified.");
    }

    if (args.output.empty()) {
        args.output = replace_extension(args.filename, "o");
    }

    if (args.entry.empty()) {
        args.entry = "main";
    }

    if (args.emit_llvm && args.emit_assembly) {
        error("Cannot emit both Assembly and LLVM IR.");
    }

    if (args.emit_llvm) {
        args.output = replace_extension(args.output, "ll");
    } else if (args.emit_assembly) {
        args.output = replace_extension(args.output, "S");
    }
    
    delete[] options;
    return args;
}

#else
    Arguments parse_arguments(int argc, char** argv);
    #error "Unsupported compiler"
#endif


int main(int argc, char** argv) {
    Arguments args = parse_arguments(argc, argv);

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    std::ifstream file(args.filename);

    Lexer lexer(file, args.filename);
    std::vector<Token> tokens = lexer.lex();

    Parser parser(tokens);
    std::unique_ptr<ast::Program> program = parser.statements();

    Visitor visitor(args.filename);
    visitor.visit(std::move(program));

    llvm::Function* entry = visitor.module->getFunction(args.entry);
    if (!entry) {
        error("Entry point '" + args.entry + "' not found.");
    }

    // TODO: Improve this.
    // Let's take the following example: 
    //
    // struct Foo {
    //    def foo(self) {}
    //    def bar(self) { self.foo() }
    // }
    //
    // Code for `Foo.foo` would be generated since it's technically used by `Foo.bar` but since
    // `Foo.bar` is never used anywhere that technically means that code for `Foo.foo` shouldn't be generated.

    for (auto& pair : visitor.functions) {
        if (!pair.second) continue;
        if (!pair.second->used) {
            if (pair.first == "main") {
                continue;
            }

            std::string name = visitor.is_intrinsic(pair.first).first;
            llvm::Function* function = visitor.module->getFunction(name);

            function->eraseFromParent();
        }

        delete pair.second;
    }

    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    std::string error;

    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, error);
    if (!target) {
        std::cerr << "Failed to create target: " << error << std::endl;
        return 1;
    }

    std::string cpu = "generic";
    std::string features = "";

    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>();

    llvm::TargetMachine* target_machine = target->createTargetMachine(target_triple, cpu, features, options, reloc);
    visitor.module->setDataLayout(target_machine->createDataLayout());
    visitor.module->setTargetTriple(target_triple);

    visitor.module->setPICLevel(llvm::PICLevel::BigPIC);
    visitor.module->setPIELevel(llvm::PIELevel::Large);

    std::error_code ec;

    llvm::raw_fd_ostream dest(args.output, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Could not open file. " << ec.message() << std::endl;
        return 1;
    }

    if (args.emit_llvm) {
        visitor.module->print(dest, nullptr);
        exit(0);   
    }

    llvm::legacy::PassManager pass;

    llvm::CodeGenFileType file_type = llvm::CodeGenFileType::CGFT_ObjectFile;
    if (args.emit_assembly) {
        file_type = llvm::CodeGenFileType::CGFT_AssemblyFile;
    }

    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, file_type)) {
        std::cerr << "Target machine can't emit a file of this type" << std::endl;
        return 1;
    }

#if _DEBUG_MODULE
    visitor.dump(llvm::outs());
#endif

    pass.run(*visitor.module);
    dest.flush();
} 