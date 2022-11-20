#include "compiler.h"
#include "utils.h"
#include "llvm.h"
#include "visitor.h"
#include "preprocessor.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include "llvm/Passes/PassBuilder.h"

#include <ratio>
#include <sstream>
#include <vector>
#include <iomanip>

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

void Compiler::add_library(std::string name) {
    this->libraries.names.push_back(name);
}

void Compiler::add_library_path(std::string path) {
    this->libraries.paths.push_back(path);
}

void Compiler::set_libraries(std::vector<std::string> names) {
    this->libraries.names = names;
}

void Compiler::set_library_paths(std::vector<std::string> paths) {
    this->libraries.paths = paths;
}

void Compiler::add_include_path(std::string path) {
    this->includes.push_back(path);
}

void Compiler::define_preprocessor_macro(std::string name, int value) {
    Location location = {0, 0, 0, "<compiler>"};
    Token token = {TokenKind::Integer, location, location, std::to_string(value)};

    this->macros.push_back(Macro(name, {}, {token}));
}

void Compiler::define_preprocessor_macro(std::string name, std::string value) {
    Location location = {0, 0, 0, "<compiler>"};
    Token token = {TokenKind::String, location, location, value};

    this->macros.push_back(Macro(name, {}, {token}));
}

void Compiler::set_output_format(OutputFormat format) {
    this->format = format;
}

void Compiler::set_output_file(std::string output) {
    this->output = output;
}

void Compiler::set_optimization_level(OptimizationLevel level) {
    this->opt = level;
}

void Compiler::set_input_file(std::string file) {
    this->input = file;
}

void Compiler::set_entry_point(std::string entry) {
    this->entry = entry;
}

void Compiler::set_target(std::string target) {
    this->target = target;
}

void Compiler::set_verbose(bool verbose) {
    this->verbose = verbose;
}

void Compiler::dump() {
    std::stringstream stream;

    stream << FORMAT("Input file: '{0}'", this->input) << '\n';
    stream << FORMAT("Output file: '{0}'", this->output) << '\n';
    stream << FORMAT("Program entry point: '{0}'", this->entry) << '\n';

    const char* opt = this->opt == OptimizationLevel::Debug ? "Debug" : "Release";
    stream << FORMAT("Optimization level: '{0}'", opt) << '\n';

    const char* format = OUTPUT_FORMATS_TO_STR[this->format];
    stream << FORMAT("Output format: '{0}'", format) << '\n';

    if (!this->target.empty()) {
        stream << FORMAT("Target: '{0}'", this->target) << '\n';
    }

    if (!this->includes.empty()) {
        stream << FORMAT("Include paths: {0}", llvm::make_range(this->includes.begin(), this->includes.end())) << '\n';
    }

    if (!this->libraries.names.empty() || !this->libraries.paths.empty()) {
        stream << "Libraries:" << '\n';
        if (!this->libraries.names.empty()) {
            auto libnames = llvm::make_range(this->libraries.names.begin(), this->libraries.names.end());
            stream << "    " << FORMAT("Library names: {0}", libnames) << '\n';
        }
        
        if (!this->libraries.paths.empty()) {
            auto libpaths = llvm::make_range(this->libraries.paths.begin(), this->libraries.paths.end());
            stream << "    " << FORMAT("Library paths: {0}", libpaths) << '\n';
        }
    }

    stream << FORMAT("Linker: '{0}'", this->linker) << '\n';
    if (!this->extras.empty()) {
        stream << "Extra linker options: " << '\n';
        for (auto& pair : this->extras) {
            stream << "    " << pair.first << " " << pair.second << '\n'; 
        }

    }

    std::cout << stream.str();
    return;
}

void Compiler::set_linker(std::string linker) {
    this->linker = linker;
}

void Compiler::add_extra_linker_option(std::string name, std::string value) {
    this->extras.push_back({name, value});
}

void Compiler::add_extra_linker_option(std::string name) {
    this->extras.push_back({name, ""});
}

std::vector<std::string> Compiler::get_linker_arguments() {
    std::string object = utils::filesystem::replace_extension(this->input, "o");
    std::vector<std::string> args = {
        this->linker,
        "-o", this->output,
    };

    if (this->entry != "main" || this->linker == "ld") {
        args.push_back("-e"); args.push_back(this->entry);
    }

    for (auto& pair : this->extras) {
        args.push_back(pair.first);
        if (!pair.second.empty()) {
            args.push_back(pair.second);
        }
    }

    args.push_back(object);

    for (auto& name : this->libraries.names) {
        args.push_back(FORMAT("-l{0}", name));
    }

    for (auto& path : this->libraries.paths) {
        args.push_back(FORMAT("-L{0}", path));
    }

    if (this->format == OutputFormat::SharedLibrary) {
        args.push_back("-shared");
    }

    return args;
}

CompilerError Compiler::compile() {
    if (this->verbose) {
        this->dump();
        std::cout << '\n';
    }

    std::fstream file(this->input, std::ios::in);
    Lexer lexer(file, this->input);

    std::vector<Token> tokens = lexer.lex();

    auto start = Compiler::now();

    Preprocessor preprocessor(tokens, this->includes);
    for (auto& macro : this->macros) {
        preprocessor.macros[macro.name] = macro;
    }

    tokens = preprocessor.process();
    if (this->verbose) {
        Compiler::log_duration("Lexing and Preprocessing", start);
    }

    Parser parser(tokens);
    if (this->verbose) {
        start = Compiler::now();
    }

    auto ast = parser.parse();
    if (this->verbose) {
        Compiler::log_duration("Parsing", start);
    }

    Visitor visitor(this->input, this->entry, bool(this->opt));
    if (this->verbose) {
        start = Compiler::now();
    }

    visitor.visit(std::move(ast));
    if (this->verbose) {
        Compiler::log_duration("Visiting the AST and generating LLVM IR", start);
        std::cout << '\n';
    }

    visitor.create_global_constructors();

    parser.free();
    visitor.finalize();

    if (this->verbose) {
        start = Compiler::now();
    }

    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    if (!this->target.empty()) {
        target_triple = this->target;
    }

    std::string err;
    if (this->verbose) {
        std::cout << "LLVM Tagret Triple: " << std::quoted(target_triple) << '\n';
    }

    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, err);
    if (!target) {
        return {1, FORMAT("Could not create target: '{0}'", err)};
    }

    if (this->verbose) {
        std::cout << "LLVM Target: " << std::quoted(target->getName()) << '\n';
    }

    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    utils::Ref<llvm::TargetMachine> machine(
        target->createTargetMachine(target_triple, "generic", "", options, reloc)
    );

    visitor.module->setDataLayout(machine->createDataLayout());
    visitor.module->setTargetTriple(target_triple);

    visitor.module->setPICLevel(llvm::PICLevel::BigPIC);
    visitor.module->setPIELevel(llvm::PIELevel::Large);

    std::error_code error;

    std::string object;
    if (this->format == OutputFormat::Executable || this->format == OutputFormat::SharedLibrary) {
        object = utils::filesystem::replace_extension(this->input, "o");
    } else {
        object = this->output;
    }

    llvm::raw_fd_ostream dest(object, error, llvm::sys::fs::OF_None);
    if (error) {
        return {1, FORMAT("Could not open file '{0}'. {1}", object, error.message())};
    }

    if (this->format == OutputFormat::LLVM) {
        visitor.module->print(dest, nullptr);
        if (this->verbose) {
            Compiler::log_duration("LLVM", start);
        }

        return {};
    } else if (this->format == OutputFormat::Bitcode) {
        llvm::BitcodeWriterPass pass(dest);
        llvm::ModuleAnalysisManager manager;

        pass.run(*visitor.module, manager);
        if (this->verbose) {
            Compiler::log_duration("LLVM", start);
        }
        
        return {};
    }

    llvm::legacy::PassManager pass;

    llvm::CodeGenFileType type = llvm::CodeGenFileType::CGFT_ObjectFile;
    if (this->format == OutputFormat::Assembly) {
        type = llvm::CodeGenFileType::CGFT_AssemblyFile;
    }

    if (machine->addPassesToEmitFile(pass, dest, nullptr, type)) {
        return {1, "Target machine can't emit a file of this type"};
    }

    pass.run(*visitor.module);
    dest.flush();

    if (this->verbose) {
        Compiler::log_duration("LLVM", start);
        std::cout << '\n';
    }

    if (this->format != OutputFormat::Executable && this->format != OutputFormat::SharedLibrary) {
        return {};
    }

    std::vector<std::string> args = this->get_linker_arguments();
    std::string command = utils::join(" ", args);

    if (this->verbose) {
        std::cout << "Linker command invokation: " << std::quoted(command) << '\n';
        start = Compiler::now();
    }

    uint32_t code = std::system(command.c_str());
    if (this->verbose) {
        Compiler::log_duration("Linking", start);
    }

    if (code != 0) {
        return {code, FORMAT("Linker exited with code {0}", code)};
    }

    return {};
}