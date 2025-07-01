#include <quart/compiler.h>
#include <quart/lexer/lexer.h>
#include <quart/parser/parser.h>
#include <quart/language/state.h>
#include <quart/language/symbol.h>
#include <quart/codegen/llvm.h>
#include <quart/target.h>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/Program.h>
#include <llvm/ADT/StringExtras.h>
#include <llvm/TargetParser/Host.h>

#include <sstream>

#define PANIC_OBJ_FILE QUART_PATH "/panic.o"

// We define our own TRY macro here because we need it to be slightly different from the one in quart/errors.h
#undef TRY
#define TRY(expr)                                           \
    ({                                                      \
        auto result = (expr);                               \
        if (result.is_err()) {                              \
            auto& err = result.error();                     \
            errln("{}", SourceCode::format_error(err));    \
                                                            \
            return 1;                                       \
        }                                                   \
        result.release_value();                             \
    })

namespace quart {

Compiler::TimePoint Compiler::now() {
    return std::chrono::high_resolution_clock::now();
}

void Compiler::dump() const {
    std::stringstream stream;

    stream << format("Input file: '{}'", m_options.file.filename()) << '\n';
    stream << format("Output file: '{}'", m_options.output) << '\n';
    stream << format("Program entry point: '{}'", m_options.entry) << '\n';

    stream << format("Optimization level: 'O{}'", (u32)m_options.opts.level) << '\n';

    StringView fmt = OUTPUT_FORMATS_TO_STR.at(m_options.format);
    stream << format("Output format: '{}'", fmt) << '\n';

    if (!m_options.target.empty()) {
        stream << format("Target: '{}'", m_options.target) << '\n';
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
        String libs = "..."; // TODO: Format library names
        stream << "    - " << format("Library names: {}", libs) << '\n';
    }

    if (!m_options.library_paths.empty()) {
        String libpaths = "..."; // TODO: Format library paths
        stream << "    - " << quart::format("Library paths: {}", libpaths) << '\n';
    }

    stream << format("Linker: '{}'", m_options.linker) << '\n';
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
    String object = m_options.file.with_extension("o");
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
        args.push_back(format("-l{}", name));
    }

    for (auto& path : m_options.library_paths) {
        args.push_back(format("-L{}", path));
    }

    if (m_options.format == OutputFormat::SharedLibrary) {
        args.emplace_back("-shared");
    }

    return args;
}

int Compiler::compile() const {
    String target = m_options.has_target() ? Target::normalize(m_options.target) : llvm::sys::getDefaultTargetTriple();
    Target::set_build_target(target);

    auto source_code = SourceCode::from_path(m_options.file);

    Lexer lexer(source_code);
    Vector<Token> tokens = TRY(lexer.lex());

    Parser parser(move(tokens));
    ast::ExprList<> ast = TRY(parser.parse());

    State state;
    for (auto& expr : ast) {
        TRY(expr->generate(state));
    }

    // for (auto& [_, function] : state.functions()) {
    //     if (function->is_decl()) continue;
    //     function->dump();
    // }
 
    codegen::LLVMCodeGen codegen(state, m_options.file.filename());
    auto result = codegen.generate(m_options);

    if (result.is_err()) {
        auto& err = result.error();
        errln("\x1b[1;37mquart: \x1b[1;31merror: \x1b[0m{}", err.message());

        return 1;
    }

    Vector<String> arguments = this->get_linker_arguments();
    String command = llvm::join(arguments, " ");

    int retcode = std::system(command.c_str());
    return retcode;
}

}