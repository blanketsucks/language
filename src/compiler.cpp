#include <quart/compiler.h>
#include <quart/visitor.h>
#include <quart/lexer.h>
#include <quart/parser.h>
#include <quart/utils/string.h>
#include <quart/jit.h>

#include <llvm/Passes/PassBuilder.h>

#include <ratio>
#include <sstream>
#include <vector>
#include <fstream>
#include <iomanip>

CompilerError::CompilerError(uint32_t code, const std::string& message) : code(code), message(message) {}

CompilerError CompilerError::success() {
    return CompilerError(0, "Success");
}

void CompilerError::unwrap() {
    if (this->code != 0) {
        Compiler::error(this->message);
        exit(this->code);
    }
}

Compiler::TimePoint Compiler::now() {
    return std::chrono::high_resolution_clock::now();
}

double Compiler::duration(Compiler::TimePoint start, Compiler::TimePoint end) {
    std::chrono::duration<double, std::milli> duration = end - start;
    return duration.count();
}

void Compiler::log_duration(const char* message, Compiler::TimePoint start) {
    std::cout << message << " took " << Compiler::duration(start, Compiler::now()) << " milliseconds.\n";
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

void Compiler::add_library(const std::string& name) { this->options.libs.names.insert(name); }
void Compiler::add_library_path(const std::string& path) { this->options.libs.paths.insert(path); }
void Compiler::set_libraries(std::set<std::string> names) { this->options.libs.names = names; }
void Compiler::set_library_paths(std::set<std::string> paths) { this->options.libs.paths = paths; }
void Compiler::add_include_path(const std::string& path) { this->options.includes.push_back(path); }
void Compiler::set_output_format(OutputFormat format) { this->options.format = format; }
void Compiler::set_output_file(const std::string& output) { this->options.output = output; }
void Compiler::set_optimization_level(OptimizationLevel level) { this->options.optimization = level; }
void Compiler::set_optimization_options(OptimizationOptions options) { this->options.opts = options; }
void Compiler::set_input_file(const utils::fs::Path& input) { this->options.input = input; }
void Compiler::set_entry_point(const std::string& entry) { this->options.entry = entry; }
void Compiler::set_target(const std::string& target) { this->options.target = target; }
void Compiler::set_verbose(bool verbose) { this->options.verbose = verbose; }
void Compiler::set_linker(const std::string& linker) { this->options.linker = linker; }
void Compiler::add_extra_linker_option(const std::string& name, const std::string& value) { this->options.extras.push_back({name, value}); }
void Compiler::add_extra_linker_option(const std::string& name) { this->options.extras.push_back({name, ""}); }
void Compiler::add_object_file(const std::string& file) { this->options.object_files.push_back(file); }

void Compiler::dump() {
    std::stringstream stream;

    stream << FORMAT("Input file: '{0}'", this->options.input.filename()) << '\n';
    stream << FORMAT("Output file: '{0}'", this->options.output) << '\n';
    stream << FORMAT("Program entry point: '{0}'", this->options.entry) << '\n';

    const char* opt = this->options.optimization == OptimizationLevel::Debug ? "Debug" : "Release";
    stream << FORMAT("Optimization level: '{0}'", opt) << '\n';

    const char* format = OUTPUT_FORMATS_TO_STR[this->options.format];
    stream << FORMAT("Output format: '{0}'", format) << '\n';

    if (!this->options.target.empty()) {
        stream << FORMAT("Target: '{0}'", this->options.target) << '\n';
    }

    if (!this->options.includes.empty()) {
        stream << FORMAT(
            "Include paths: {0}", llvm::make_range(this->options.includes.begin(), this->options.includes.end())
        ) << '\n';
    }

    if (!this->options.libs.empty()) {
        stream << "Libraries:" << '\n';
    }

    if (!this->options.libs.names.empty()) {
        auto libnames = llvm::make_range(this->options.libs.names.begin(), this->options.libs.names.end());
        stream << "    " << FORMAT("Library names: {0}", libnames) << '\n';
    }
        
    if (!this->options.libs.paths.empty()) {
        auto libpaths = llvm::make_range(this->options.libs.paths.begin(), this->options.libs.paths.end());
        stream << "    " << FORMAT("Library paths: {0}", libpaths) << '\n';
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
    std::string object = this->options.input.with_extension("o").str();
    std::vector<std::string> args = {
        this->options.linker,
        "-o", this->options.output,
    };

    if (this->options.entry != "main" || this->options.linker == "ld") {
        args.push_back("-e"); args.push_back(this->options.entry);
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

    for (auto& name : this->options.libs.names) {
        args.push_back(FORMAT("-l{0}", name));
    }

    for (auto& path : this->options.libs.paths) {
        args.push_back(FORMAT("-L{0}", path));
    }

    if (this->options.format == OutputFormat::SharedLibrary) {
        args.push_back("-shared");
    }

    return args;
}

CompilerError Compiler::compile() {
    if (this->options.verbose) {
        this->dump();
        std::cout << '\n';
    }

    Lexer lexer(this->options.input);
    TimePoint start = Compiler::now();

    std::vector<Token> tokens = lexer.lex();
    if (this->options.verbose) {
        Compiler::log_duration("Lexing", start);
    }

    Parser parser(tokens);
    if (this->options.verbose) {
        start = Compiler::now();
    }

    auto ast = parser.parse();
    if (this->options.verbose) {
        Compiler::log_duration("Parsing", start);
    }

    Visitor visitor(this->options.input.str(), this->options);
    if (this->options.verbose) {
        start = Compiler::now();
    }

    visitor.visit(std::move(ast));
    if (this->options.verbose) {
        Compiler::log_duration("Visiting the AST and generating LLVM IR", start);
        std::cout << '\n';
    }

    visitor.create_global_constructors();
    visitor.finalize();

    if (visitor.link_panic) {
        this->add_library("pthread");
        this->add_object_file("lib/panic.o");
    }

    if (this->options.verbose) {
        start = Compiler::now();
    } 

    std::string triple = this->options.has_target() ? this->options.target : llvm::sys::getDefaultTargetTriple();
    std::string err;

    if (this->options.verbose) {
        std::cout << "LLVM Tagret Triple: " << std::quoted(triple) << '\n';
    }

    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, err);
    if (!target) {
        return CompilerError(1, FORMAT("Could not create target: '{0}'", err));
    }

    if (this->options.verbose) {
        std::cout << "LLVM Target: " << std::quoted(target->getName()) << '\n';
    }

    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    utils::Scope<llvm::TargetMachine> machine(
        target->createTargetMachine(triple, "generic", "", options, reloc)
    );

    visitor.module->setDataLayout(machine->createDataLayout());
    visitor.module->setTargetTriple(triple);

    visitor.module->setPICLevel(llvm::PICLevel::BigPIC);
    visitor.module->setPIELevel(llvm::PIELevel::Large);

    std::error_code error;

    std::string object;
    if (this->options.format == OutputFormat::Executable || this->options.format == OutputFormat::SharedLibrary) {
        object = this->options.input.with_extension("o").str();
    } else {
        object = this->options.output;
    }

    llvm::raw_fd_ostream dest(object, error, llvm::sys::fs::OF_None);
    if (error) {
        return CompilerError(1, FORMAT("Could not open file '{0}': {1}", object, error.message()));
    }

    if (this->options.format == OutputFormat::LLVM) {
        visitor.module->print(dest, nullptr);
        if (this->options.verbose) {
            Compiler::log_duration("LLVM", start);
        }

        return CompilerError::success();
    } else if (this->options.format == OutputFormat::Bitcode) {
        llvm::BitcodeWriterPass pass(dest);
        llvm::ModuleAnalysisManager manager;

        pass.run(*visitor.module, manager);
        if (this->options.verbose) {
            Compiler::log_duration("LLVM", start);
        }
        
        return CompilerError::success();
    }

    llvm::legacy::PassManager pass;

    llvm::CodeGenFileType type = (
        this->options.format == OutputFormat::Assembly ? llvm::CodeGenFileType::CGFT_AssemblyFile : llvm::CodeGenFileType::CGFT_ObjectFile
    );

    if (machine->addPassesToEmitFile(pass, dest, nullptr, type)) {
        return CompilerError(1, "Target machine can't emit a file of this type");
    }

    pass.run(*visitor.module);
    dest.flush();

    if (this->options.verbose) {
        Compiler::log_duration("LLVM", start);
        std::cout << '\n';
    }

    if (this->options.format != OutputFormat::Executable && this->options.format != OutputFormat::SharedLibrary) {
        return CompilerError::success();
    }

    std::vector<std::string> args = this->get_linker_arguments();
    std::string command = utils::join(" ", args);

    if (this->options.verbose) {
        std::cout << "Linker command: " << std::quoted(command) << '\n';
        start = Compiler::now();
    }
    
    int code = std::system(command.c_str());
    if (this->options.verbose) {
        Compiler::log_duration("Linking", start);
    }
    
    if (code != 0) {
        return CompilerError(code, FORMAT("Linker exited with code {0}", code));
    }

    return CompilerError::success();
}

int Compiler::jit(int argc, char** argv) {
    Lexer lexer(this->options.input);
    auto tokens = lexer.lex();

    Parser parser(tokens);
    auto ast = parser.parse();

    Visitor visitor(this->options.input, options);
    visitor.visit(std::move(ast));

    auto& entry = visitor.global_scope->functions[this->options.entry];
    if (!entry) {
        Compiler::error("Missing main entry point function"); exit(1);
    }

    if (entry->args.size() > 2) {
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