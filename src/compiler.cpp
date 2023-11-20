#include <quart/compiler.h>
#include <quart/visitor.h>
#include <quart/lexer.h>
#include <quart/parser.h>
#include <quart/jit.h>

#include <llvm/Passes/PassBuilder.h>

#include <ratio>
#include <sstream>
#include <vector>
#include <fstream>
#include <iomanip>

#define PANIC_OBJ_FILE QUART_PATH "/panic.o"

#define VERBOSE(msg) if (this->options.verbose) Compiler::debug(msg, start)

using namespace quart;

CompilerError::CompilerError(i32 code, std::string message) : code(code), message(std::move(message)) {}

CompilerError CompilerError::ok() {
    return { 0, "Success" };
}

void CompilerError::unwrap() {
    if (this->code != 0) {
        Compiler::error(this->message);
        exit(static_cast<i32>(this->code));
    }
}

Compiler::TimePoint Compiler::now() {
    return std::chrono::high_resolution_clock::now();
}

double Compiler::duration(Compiler::TimePoint start, Compiler::TimePoint end) {
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

void Compiler::debug(const char* message, Compiler::TimePoint start) {
    double duration = Compiler::duration(start, Compiler::now());
    if (duration < 1000) {
        std::cout << message << " took " << duration << " milliseconds.\n";
    } else {
        std::cout << message << " took " << duration / 1000 << " seconds.\n";
    }
}

void Compiler::init() {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
}

void Compiler::shutdown() {
    llvm::llvm_shutdown();
}

void Compiler::add_library(const std::string& name) { this->options.library_names.insert(name); }
void Compiler::add_library_path(const std::string& path) { this->options.library_paths.insert(path); }

void Compiler::set_libraries(std::set<std::string> names) { this->options.library_names = std::move(names); }
void Compiler::set_library_paths(std::set<std::string> paths) { this->options.library_paths = std::move(paths); }

void Compiler::add_import_path(const std::string& path) { this->options.imports.push_back(path); }

void Compiler::set_output_format(OutputFormat format) { this->options.format = format; }
void Compiler::set_output_file(const std::string& output) { this->options.output = output; }

void Compiler::set_optimization_level(OptimizationLevel level) { this->options.opts.level = level; }
void Compiler::set_optimization_options(const OptimizationOptions& opts) { this->options.opts = opts; }

void Compiler::set_input_file(const fs::Path& input) { this->options.input = input; }
void Compiler::set_entry_point(const std::string& entry) { this->options.entry = entry; }
void Compiler::set_target(const std::string& target) { this->options.target = target; }
void Compiler::set_verbose(bool verbose) { this->options.verbose = verbose; }
void Compiler::set_linker(const std::string& linker) { this->options.linker = linker; }

void Compiler::add_extra_linker_option(const std::string& name, const std::string& value) { 
    this->options.extras.emplace_back(name, value);
}

void Compiler::add_extra_linker_option(const std::string& name) { 
    this->options.extras.emplace_back(name, "");
}

void Compiler::add_object_file(const std::string& file) { this->options.object_files.push_back(file); }

void Compiler::dump() {
    std::stringstream stream;

    stream << FORMAT("Input file: '{0}'", this->options.input.filename()) << '\n';
    stream << FORMAT("Output file: '{0}'", this->options.output) << '\n';
    stream << FORMAT("Program entry point: '{0}'", this->options.entry) << '\n';

    const char* opt = this->options.opts.level == OptimizationLevel::Debug ? "Debug" : "Release";
    stream << FORMAT("Optimization level: '{0}'", opt) << '\n';

    llvm::StringRef format = OUTPUT_FORMATS_TO_STR.at(this->options.format);
    stream << FORMAT("Output format: '{0}'", format) << '\n';

    if (!this->options.target.empty()) {
        stream << FORMAT("Target: '{0}'", this->options.target) << '\n';
    }

    if (!this->options.imports.empty()) {
        stream << "Import paths:" << '\n';

        for (auto& path : this->options.imports) {
            stream << "    - " << path << '\n';
        }
    }

    if (!this->options.library_names.empty() || !this->options.library_paths.empty()) {
        stream << "Libraries:" << '\n';
    }

    if (!this->options.library_names.empty()) {
        auto libnames = llvm::make_range(this->options.library_names.begin(), this->options.library_names.end());
        stream << "    - " << FORMAT("Library names: {0}", libnames) << '\n';
    }
        
    if (!this->options.library_paths.empty()) {
        auto libpaths = llvm::make_range(this->options.library_paths.begin(), this->options.library_paths.end());
        stream << "    - " << FORMAT("Library paths: {0}", libpaths) << '\n';
    }

    stream << FORMAT("Linker: '{0}'", this->options.linker) << '\n';
    if (!this->options.extras.empty()) {
        stream << "Extra linker options: " << '\n';
        for (auto& pair : this->options.extras) {
            stream << "    " << pair.first << " " << pair.second << '\n'; 
        }

    }

    std::cout << stream.str();
    return;
}

std::vector<std::string> Compiler::get_linker_arguments() {
    std::string object = this->options.input.with_extension("o");
    std::vector<std::string> args = {
        this->options.linker,
        "-o", this->options.output,
    };

    if (this->options.entry != "main" || this->options.linker == "ld") {
        args.emplace_back("-e"); 
        args.push_back(this->options.entry);
    }

    for (auto& pair : this->options.extras) {
        args.push_back(pair.first);
        if (!pair.second.empty()) {
            args.push_back(pair.second);
        }
    }

    args.push_back(object);
    for (auto& file : this->options.object_files) {
        args.push_back(file);
    }

    for (auto& name : this->options.library_names) {
        args.push_back(FORMAT("-l{0}", name));
    }

    for (auto& path : this->options.library_paths) {
        args.push_back(FORMAT("-L{0}", path));
    }

    if (this->options.format == OutputFormat::SharedLibrary) {
        args.emplace_back("-shared");
    }

    return args;
}

const llvm::Target* Compiler::create_target(
    std::string& error, std::string& triple
) {
    triple = this->options.has_target() ? this->options.target : llvm::sys::getDefaultTargetTriple();
    return llvm::TargetRegistry::lookupTarget(triple, error);
}

OwnPtr<llvm::TargetMachine> Compiler::create_target_machine(
    llvm::Module& module, const llvm::Target* target, llvm::StringRef triple
) {
    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    OwnPtr<llvm::TargetMachine> machine(
        target->createTargetMachine(triple, "generic", "", options, reloc)
    );

    module.setDataLayout(machine->createDataLayout());
    module.setTargetTriple(triple);

    module.setPICLevel(llvm::PICLevel::BigPIC);
    module.setPIELevel(llvm::PIELevel::Large);

    return machine;
}

CompilerError Compiler::compile() {
    if (this->options.verbose) {
        this->dump();
        std::cout << '\n';
    }

    MemoryLexer lexer(this->options.input);
    TimePoint start = Compiler::now();

    std::vector<Token> tokens = lexer.lex();
    VERBOSE("Lexing");

    Parser parser(std::move(tokens));
    start = Compiler::now();

    auto ast = parser.parse();
    VERBOSE("Parsing");

    Visitor visitor(this->options.input, this->options);
    start = Compiler::now();

    visitor.visit(std::move(ast));
    VERBOSE("Visiting the AST");

    visitor.create_global_constructors();
    visitor.finalize();

    if (visitor.link_panic) {
        this->add_library("pthread");
        this->add_object_file(PANIC_OBJ_FILE);
    }

    start = Compiler::now();

    std::string err, triple;
    const llvm::Target* target = this->create_target(err, triple);

    if (!target) return { 1, FORMAT("Could not create target: '{0}'", err) };

    if (this->options.verbose) { std::cout << "\nLLVM Target Triple: " << std::quoted(triple) << '\n'; }
    OwnPtr<llvm::TargetMachine> machine = this->create_target_machine(
        *visitor.module, target, triple
    );

    std::error_code error;

    std::string object;
    if (this->options.format == OutputFormat::Executable || this->options.format == OutputFormat::SharedLibrary) {
        object = this->options.input.with_extension("o");
    } else {
        object = this->options.output;
    }

    llvm::raw_fd_ostream dest(object, error, llvm::sys::fs::OF_None);
    if (error) {
        return { 1, FORMAT("Could not open file '{0}': {1}", object, error.message()) };
    }

    llvm::CodeGenFileType type = llvm::CodeGenFileType::CGFT_ObjectFile;
    switch (this->options.format) {
        case OutputFormat::LLVM:
            visitor.module->print(dest, nullptr);
            VERBOSE("Emitting LLVM IR");

            return CompilerError::ok();
        case OutputFormat::Bitcode: {
            llvm::BitcodeWriterPass pass(dest);
            llvm::ModuleAnalysisManager manager;

            pass.run(*visitor.module, manager);
            dest.flush();

            VERBOSE("Emitting LLVM Bitcode");
            return CompilerError::ok();
        }
        case OutputFormat::Assembly:
            type = llvm::CodeGenFileType::CGFT_AssemblyFile;
        default: break;
    }

    llvm::legacy::PassManager pass;
    if (machine->addPassesToEmitFile(pass, dest, nullptr, type)) {
        return { 1, "Target machine can't emit a file of this type" };
    }

    pass.run(*visitor.module);
    dest.flush();

    VERBOSE("Compilation");
    if (this->options.format != OutputFormat::Executable && this->options.format != OutputFormat::SharedLibrary) {
        return CompilerError::ok();
    }

    std::vector<std::string> args = this->get_linker_arguments();
    std::string command = llvm::join(args, " ");

    if (this->options.verbose) {
        std::cout << "\nLinker command: " << std::quoted(command) << '\n';
        start = Compiler::now();
    }
    
    i32 code = std::system(command.c_str());
    VERBOSE("Linking");
    
    if (code != 0) {
        return { code, FORMAT("Linker exited with code {0}", code) };
    }

    return CompilerError::ok();
}

int Compiler::jit(int argc, char** argv) {
    MemoryLexer lexer(this->options.input);
    auto tokens = lexer.lex();

    Parser parser(tokens);
    auto ast = parser.parse();

    Visitor visitor(this->options.input, options);
    visitor.visit(std::move(ast));

    auto& entry = visitor.global_scope->functions[this->options.entry];
    if (!entry) {
        Compiler::error("Missing main entry point function"); exit(1);
    }

    if (entry->params.size() > 2) {
        Compiler::error("Main entry point function takes no more than 2 arguments"); exit(1);
    }

    // Apparently, we can only access the global ctor function only if it has the external linkage
    visitor.create_global_constructors(llvm::Function::ExternalLinkage);
    visitor.finalize();

    jit::QuartJIT jit = jit::QuartJIT(
        this->options.input,
        this->options.entry,
        std::move(visitor.module), 
        std::move(visitor.context)
    );

    return jit.run(argc, argv);
}