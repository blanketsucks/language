#include "compiler.h"

#include "utils.h"
#include "llvm.h"
#include "visitor.h"
#include "preprocessor.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "llvm/ADT/Triple.h"
#include "llvm/MC/TargetRegistry.h"

#include <sstream>
#include <vector>

void Compiler::init() {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
}

void Compiler::error(const std::string& str, ...) {
    va_list args;
    va_start(args, str);

    std::string message = utils::fmt::format("{bold|white} {bold|red} {s}", "proton:", "error:", str);
    std::string result = utils::fmt::format(message, args);

    va_end(args);
    std::exit(1);
}

void Compiler::add_library(std::string name) {
    this->libraries.names.push_back(name);
}

void Compiler::add_library_path(std::string path) {
    this->libraries.paths.push_back(path);
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

void Compiler::dump() {
    std::stringstream stream;

    stream << utils::fmt::format("Input file: '{s}'", this->input) << '\n';
    stream << utils::fmt::format("Output file: '{s}'", this->output) << '\n';
    stream << utils::fmt::format("Program entry point: '{s}'", this->entry) << '\n';

    const char* opt = this->opt == OptimizationLevel::Debug ? "Debug" : "Release";
    stream << utils::fmt::format("Optimization level: '{}'", opt) << '\n';

    const char* format = OUTPUT_FORMATS_TO_STR[this->format];
    stream << utils::fmt::format("Output format: '{}'", format) << '\n';

    if (!this->includes.empty()) {
        std::string includes = utils::fmt::join(", ", this->includes);
        stream << utils::fmt::format("Include paths: {s}", includes) << '\n';
    }

    if (!this->libraries.names.empty() || !this->libraries.paths.empty()) {
        stream << "Libraries:" << '\n';
        if (!this->libraries.names.empty()) {
            std::string libnames = utils::fmt::join(", ", this->libraries.names);
            stream << "    " << utils::fmt::format("Library names: {s}", libnames) << '\n';
        }
        
        if (!this->libraries.paths.empty()) {
            std::string libpaths = utils::fmt::join(", ", this->libraries.paths);
            stream << "    " << utils::fmt::format("Library paths: {s}", libpaths) << '\n';
        }
    }

    stream << utils::fmt::format("Linker: '{s}'", this->linker) << '\n';
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
    std::string object = this->output;
    if (!utils::filesystem::has_extension(object)) {
        object += ".o";
    }

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
        args.push_back(utils::fmt::format("-l{s}", name));
    }

    for (auto& path : this->libraries.paths) {
        args.push_back(utils::fmt::format("-L{s}", path));
    }

    return args;
}

CompilerError Compiler::compile() {
    std::fstream file(this->input, std::ios::in);
    Lexer lexer(file, this->input);

    Preprocessor preprocessor(lexer.lex(), this->includes);
    for (auto& macro : this->macros) {
        preprocessor.macros[macro.name] = macro;
    }

    auto tokens = preprocessor.process();

    Parser parser(tokens);
    auto ast = parser.parse();

    Visitor visitor(this->input, this->entry, bool(this->opt));
    visitor.visit(std::move(ast));

    visitor.cleanup();
    visitor.free();

    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    std::string err;

    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, err);
    if (!target) {
        return {1, utils::fmt::format("Could not create target: '{s}'", err)};
    }

    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    llvm::TargetMachine* target_machine = target->createTargetMachine(target_triple, "generic", "", options, reloc, llvm::None, llvm::CodeGenOpt::Aggressive);
    
    visitor.module->setDataLayout(target_machine->createDataLayout());
    visitor.module->setTargetTriple(target_triple);

    visitor.module->setPICLevel(llvm::PICLevel::BigPIC);
    visitor.module->setPIELevel(llvm::PIELevel::Large);

    std::error_code error;

    std::string object = this->output;
    if (!utils::filesystem::has_extension(object)) {
        object = object + ".o";
    }

    llvm::raw_fd_ostream dest(object, error, llvm::sys::fs::OF_None);
    if (error) {
        return {1, utils::fmt::format("Could not open file '{s}'. {s}", object, error.message())};
    }

    if (this->format == OutputFormat::LLVM) {
        visitor.module->print(dest, nullptr);
        delete target_machine;

        return {0, ""};
    } else if (this->format == OutputFormat::Bitcode) {
        llvm::BitcodeWriterPass pass(dest);
        llvm::ModuleAnalysisManager manager;

        pass.run(*visitor.module, manager);

        delete target_machine;
        return {0, ""};
    }

    llvm::legacy::PassManager pass;

    llvm::CodeGenFileType type = llvm::CodeGenFileType::CGFT_ObjectFile;
    if (this->format == OutputFormat::Assembly) {
        type = llvm::CodeGenFileType::CGFT_AssemblyFile;
    }

    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, type)) {
        return {1, "Target machine can't emit a file of this type"};
    }

    pass.run(*visitor.module);
    dest.flush();

    delete target_machine;
    if (this->format != OutputFormat::Executable) {
        return {0, ""};
    }

    std::vector<std::string> args = this->get_linker_arguments();

    std::string command = utils::fmt::join(" ", args);
    return {std::system(command.c_str()), ""};
}