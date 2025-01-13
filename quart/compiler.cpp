#include <llvm-17/llvm/TargetParser/Host.h>
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

#include <sstream>

#define PANIC_OBJ_FILE QUART_PATH "/panic.o"

// We define our own TRY macro here because we need it to be slightly different from the one in quart/errors.h
#undef TRY
#define TRY(expr)                                           \
    ({                                                      \
        auto result = (expr);                               \
        if (result.is_err()) {                              \
            auto& err = result.error();                     \
            errln("{0}", SourceCode::format_error(err));    \
                                                            \
            return 1;                                       \
        }                                                   \
        result.release_value();                             \
    })

namespace quart {

Compiler::TimePoint Compiler::now() {
    return std::chrono::high_resolution_clock::now();
}

void Compiler::shutdown() {
    llvm::llvm_shutdown();
}

void Compiler::dump() const {
    std::stringstream stream;

    stream << format("Input file: '{0}'", m_options.input.filename()) << '\n';
    stream << format("Output file: '{0}'", m_options.output) << '\n';
    stream << format("Program entry point: '{0}'", m_options.entry) << '\n';

    stream << format("Optimization level: 'O{0}'", (u32)m_options.opts.level) << '\n';

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

int Compiler::compile() const {
    String target = m_options.has_target() ? Target::normalize(m_options.target) : llvm::sys::getDefaultTargetTriple();
    Target::set_build_target(target);

    auto& code = SourceCode::from_path(m_options.input);

    Lexer lexer(code);
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
 
    codegen::LLVMCodeGen codegen(state, m_options.input.filename());
    auto result = codegen.generate(m_options);

    if (result.is_err()) {
        auto& err = result.error();
        errln("\x1b[1;37mquart: \x1b[1;31merror: \x1b[0m{0}", err.message());

        return 1;
    }

    Vector<String> arguments = this->get_linker_arguments();
    String command = llvm::join(arguments, " ");

    int retcode = std::system(command.c_str());
    return retcode;
}

}