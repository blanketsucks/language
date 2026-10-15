#include <quart/compiler.h>
#include <quart/lexer/lexer.h>
#include <quart/parser/parser.h>
#include <quart/parser/analysis/pass.h>
#include <quart/parser/imports.h>
#include <quart/language/state.h>
#include <quart/language/symbol.h>
#include <quart/codegen/codegen.h>
#include <quart/codegen/llvm/codegen.h>
#include <quart/target.h>

#include <quart/parser/analysis/name_resolution.h>
#include <quart/bytecode/passes/eliminate_unreachable_blocks.h>

#include <llvm/TargetParser/Host.h>

#include <sstream>

#define PANIC_OBJ_FILE QUART_PATH "/panic.o"

static constexpr bool DEBUG = true;

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

    switch (m_options.backend) {
        case Backend::Native:
            stream << "Backend: Native" << '\n'; break;
        case Backend::LLVM:
            stream << "Backedn: LLVM" << '\n'; break;
    }

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
        String libs = format_range(m_options.library_names);
        stream << "    - " << format("Library names: {}", libs) << '\n';
    }

    if (!m_options.library_paths.empty()) {
        String libpaths = format_range(m_options.library_paths);
        stream << "    - " << format("Library paths: {}", libpaths) << '\n';
    }

    stream << format("Linker: '{}'", m_options.linker) << '\n';
    if (!m_options.extras.empty()) {
        stream << "Extra linker options: " << '\n';
        for (auto& pair : m_options.extras) {
            stream << "    " << pair.first << " " << pair.second << '\n'; 
        }

    }

    std::cout << stream.str() << '\n';
    return;
}

Vector<String> Compiler::get_linker_arguments() const {
    String object = fs::with_extension(m_options.file, ".o");
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

void Compiler::run_bytecode_passes(State& state) const {
    auto passes = bytecode::PassManager::create_default();
    for (auto& [_, function] : state.functions()) {
        if (function->is_decl()) {
            continue;
        }

        passes.run(function.get());

        if constexpr (DEBUG) {
            function->dump();
            outln();
        }
    }
}

static ErrorOr<void> ensure_no_errors(State& state) {
    auto& diag = state.diagnostics();
    if (diag.empty()) {
        return {};
    }

    for (auto& err : diag.errors()) {
        errln("{}", SourceCode::format_error(err));
    }

    for (auto& note : diag.notes()) {
        errln("{}", SourceCode::format_note(note.span, note.note));
    }

    if (!diag.has_errors()) {
        diag.clear();
        return {};
    }

    return Error({}, "Compilation failed.");
}

int Compiler::run_ast_passes(OwnPtr<ast::ParseModule> module, State& state) const {
    StaticSemanticsPassManager pm;
    pm.set_after_pass_callback([&state]() -> ErrorOr<void> { return ensure_no_errors(state); });

    ImportLoader loader(state);
    Program program = TRY(loader.load_imports(*module));

    program.add_module(move(module));
    TRY(ensure_no_errors(state));
    
    pm.add<NameResolutionPass>();

    TRY(pm.run(program, state));

    return 0;
}

int Compiler::compile() const {
    String target = m_options.has_target() ? Target::normalize(m_options.target) : ::llvm::sys::getDefaultTargetTriple();
    Target::set_build_target(target);

    auto source_code = SourceCode::from_path(m_options.file);

    Lexer lexer(source_code);
    Vector<Token> tokens = TRY(lexer.lex());

    Parser parser(move(tokens));
    auto module = TRY(parser.parse());

    State state;
    if (this->run_ast_passes(move(module), state)) {
        return 1;
    }

#if 0
    for (auto& stmt : prog->stmts()) {
        TRY(stmt->generate(state));
    }

    this->run_bytecode_passes(state);

    CodeGenType cg_type = m_options.backend == Backend::Native ? CodeGenType::x86_64 : CodeGenType::LLVM;

    auto codegen = CodeGen::create(state, cg_type, m_options.file.filename());
    auto result = codegen->generate(m_options);
    if (result.is_err()) {
        auto& err = result.error();
        errln("\x1b[1;37mquart: \x1b[1;31merror: \x1b[0m{}", err.message());

        return 1;
    }

    switch (cg_type) {
        case CodeGenType::LLVM: {
            Vector<String> arguments = this->get_linker_arguments();
            String command = join(arguments, " ");

            int retcode = std::system(command.c_str());
            return retcode;
        }
        case CodeGenType::x86_64: {}
    }
#endif

    return 0;
}

}