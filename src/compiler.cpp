#include <quart/compiler.h>
#include <quart/lexer/lexer.h>
#include <quart/parser/parser.h>
#include <quart/jit.h>

#include <llvm/Passes/PassBuilder.h>

#include <ratio>
#include <sstream>

#define PANIC_OBJ_FILE QUART_PATH "/panic.o"

#define VERBOSE(msg) if (m_options.verbose) Compiler::debug(msg, start)

namespace quart {

CompilerError::CompilerError(i32 code, String message) : code(code), message(std::move(message)) {}

CompilerError CompilerError::ok() {
    return { 0, "Success" };
}

// void CompilerError::unwrap() {
//     if (this->code != 0) {
//         Compiler::error(this->message);
//         exit(static_cast<i32>(this->code));
//     }
// }

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

// void Compiler::init() {
//     llvm::InitializeAllTargetInfos();
//     llvm::InitializeAllTargets();
//     llvm::InitializeAllTargetMCs();
//     llvm::InitializeAllAsmParsers();
//     llvm::InitializeAllAsmPrinters();
// }

void Compiler::shutdown() {
    llvm::llvm_shutdown();
}

void Compiler::dump() const {
    std::stringstream stream;

    stream << format("Input file: '{0}'", m_options.input.filename()) << '\n';
    stream << format("Output file: '{0}'", m_options.output) << '\n';
    stream << format("Program entry point: '{0}'", m_options.entry) << '\n';

    const char* opt = m_options.opts.level == OptimizationLevel::Debug ? "Debug" : "Release";
    stream << format("Optimization level: '{0}'", opt) << '\n';

    StringView fmt = OUTPUT_FORMATS_TO_STR.at(m_options.format);
    stream << format("Output format: '{0}'", fmt) << '\n';

    if (!m_options.target.empty()) {
        stream << format("Target: '{0}'", m_options.target) << '\n';
    }

    if (!m_options.imports.empty()) {
        stream << "Import paths:" << '\n';

        for (auto& path : m_options.imports) {
            stream << "    - " << path << '\n';
        }
    }

    if (!m_options.library_names.empty() || !m_options.library_paths.empty()) {
        stream << "Libraries:" << '\n';
    }

    if (!m_options.library_names.empty()) {
        auto libnames = llvm::make_range(m_options.library_names.begin(), m_options.library_names.end());
        stream << "    - " << quart::format("Library names: {0}", libnames) << '\n';
    }
        
    if (!m_options.library_paths.empty()) {
        auto libpaths = llvm::make_range(m_options.library_paths.begin(), m_options.library_paths.end());
        stream << "    - " << quart::format("Library paths: {0}", libpaths) << '\n';
    }

    stream << format("Linker: '{0}'", m_options.linker) << '\n';
    if (!m_options.extras.empty()) {
        stream << "Extra linker options: " << '\n';
        for (auto& pair : m_options.extras) {
            stream << "    " << pair.first << " " << pair.second << '\n'; 
        }

    }

    std::cout << stream.str();
    return;
}

Vector<String> Compiler::get_linker_arguments() const {
    String object = m_options.input.with_extension("o");
    Vector<String> args = {
        m_options.linker,
        "-o", m_options.output,
    };

    if (m_options.entry != "main" || m_options.linker == "ld") {
        args.emplace_back("-e"); 
        args.push_back(m_options.entry);
    }

    for (auto& pair : m_options.extras) {
        args.push_back(pair.first);
        if (!pair.second.empty()) {
            args.push_back(pair.second);
        }
    }

    args.push_back(object);
    for (auto& file : m_options.object_files) {
        args.push_back(file);
    }

    for (auto& name : m_options.library_names) {
        args.push_back(format("-l{0}", name));
    }

    for (auto& path : m_options.library_paths) {
        args.push_back(format("-L{0}", path));
    }

    if (m_options.format == OutputFormat::SharedLibrary) {
        args.emplace_back("-shared");
    }

    return args;
}

// const llvm::Target* Compiler::create_target(
//     String& error, String& triple
// ) {
//     triple = m_options.has_target() ? m_options.target : llvm::sys::getDefaultTargetTriple();
//     return llvm::TargetRegistry::lookupTarget(triple, error);
// }

// OwnPtr<llvm::TargetMachine> Compiler::create_target_machine(
//     llvm::Module& module, const llvm::Target* target, llvm::StringRef triple
// ) {
//     llvm::TargetOptions options;
//     auto reloc = std::optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

//     OwnPtr<llvm::TargetMachine> machine(
//         target->createTargetMachine(triple, "generic", "", options, reloc)
//     );

//     module.setDataLayout(machine->createDataLayout());
//     module.setTargetTriple(triple);

//     module.setPICLevel(llvm::PICLevel::BigPIC);
//     module.setPIELevel(llvm::PIELevel::Large);

//     return machine;
// }

int Compiler::compile() const {
    SourceCode code = SourceCode::from_path(m_options.input);

    Lexer lexer(code);
    auto result = lexer.lex();

    if (result.is_err()) {
        auto& error = result.error();
        errln("{0}", code.format_error(error));
        
        return 1;
    }

    auto tokens = result.release_value();
    Parser parser(tokens);

    auto ast = parser.parse();
    if (ast.is_err()) {
        auto& error = ast.error();
        errln("{0}", code.format_error(error));

        return 1;
    }

    return 0;
}

}