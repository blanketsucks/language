#include <quart/parser/imports.h>
#include <quart/parser/parser.h>
#include <quart/lexer/lexer.h>
#include <quart/state.h>
#include <filesystem>

namespace quart {

ImportLoader::ImportLoader(State& state, const Vector<fs::Path>& import_search_paths) : m_state(&state) {
    m_import_search_paths.reserve(import_search_paths.size());
    for (auto& p : import_search_paths) {
        m_import_search_paths.push_back(std::filesystem::weakly_canonical(p));
    }
}

fs::Path ImportLoader::search_import_paths(const fs::Path& path) const {
    for (auto& ipath : m_import_search_paths) {
        auto fullpath = ipath / path;
        if (std::filesystem::exists(fullpath)) {
            return fullpath;
        }
    }

    auto fullpath = QUART_PATH / path;
    if (std::filesystem::exists(fullpath)) {
        return fullpath;
    }

    return {};
}

RefPtr<Module> ImportLoader::lookup(const fs::Path& path) const {
    auto iterator = m_modules.find(path);
    if (iterator == m_modules.end()) {
        return nullptr;
    }

    return iterator->second;
}

ErrorOr<fs::Path> ImportLoader::resolve_import_path(ast::ImportExpr const& import) {
    auto& path = import.path();
    auto key = make_key(path);

    auto iterator = m_paths.find(key);
    if (iterator != m_paths.end()) {
        return iterator->second;
    }

    fs::Path fullpath = {};
    for (auto& segment : path.segments()) {
        auto& name = segment.name();
        fullpath = fullpath / name;

        if (!std::filesystem::exists(fullpath)) {
            fullpath = this->search_import_paths(fullpath);
            if (fullpath.empty()) {
                m_state->add_error_diagnostic(err(import.span(), "Could not find module '{}'", path.name()));
                return {};
            }
        }

        if (!std::filesystem::is_directory(fullpath)) {
            m_state->add_error_diagnostic(err(import.span(), "Expected '{}' to be a directory", fullpath));
            return {};
        }

        if (!m_modules.contains(fullpath)) {
            auto scope = Scope::create({}, ScopeType::Module);
            auto module = Module::create(segment.name(), fullpath, fullpath, scope);
        
            m_modules[fullpath] = module;
        }
    }

    fullpath /= path.name();
    fullpath.replace_extension(".qr");

    if (!std::filesystem::exists(fullpath)) {
        fs::Path directory = fs::with_extension(fullpath);
        if (!std::filesystem::exists(directory)) {
            directory = this->search_import_paths(directory);
            if (directory.empty()) {
                m_state->add_error_diagnostic(err(import.span(), "Could not find module '{}'", path.name()));
                return {};
            }
        }

        if (!std::filesystem::is_directory(directory)) {
            m_state->add_error_diagnostic(err(import.span(), "Expected '{}' to be a directory", directory));
            return {};
        }

        fullpath = (directory /= "module.qr");
        auto status = std::filesystem::status(fullpath);
    
        if (!std::filesystem::exists(status)) {
            m_state->add_error_diagnostic(err(import.span(), "Could not find module '{}'", path.name()));
            return {};
        } else if (!std::filesystem::is_regular_file(status)) {
            m_state->add_error_diagnostic(err(import.span(), "Expected '{}' to be a regular file", fullpath));
            return {};
        }
    }

    auto scope = Scope::create({}, ScopeType::Module);
    auto module = Module::create(path.name(), fullpath, fullpath, scope);

    m_modules[fullpath] = module;
    m_paths[key] = fullpath;

    return fullpath;
}

ErrorOr<OwnPtr<ast::ParseModule>> ImportLoader::parse_file(const fs::Path& path) {
    auto source_code = SourceCode::from_path(path);

    Lexer lexer(source_code);
    Vector<Token> tokens = TRY(lexer.lex());

    Parser parser(move(tokens));
    return parser.parse();
}

auto ImportLoader::make_key(const Path& p) -> PathKey {
    Vector<String> segments;
    segments.reserve(p.segments().size());

    for (auto& s : p.segments()) {
        segments.push_back(s.name());
    }

    return { p.name(), move(segments) };
}

ErrorOr<Program> ImportLoader::load_imports(ast::ParseModule const& module) {
    Deque<fs::Path> queue;
    for (auto& import : module.imports()) {
        queue.push_back(TRY(this->resolve_import_path(*import)));
    }

    Program program;
    while (!queue.empty()) {
        auto path = std::move(queue.front());
        queue.pop_front();

        if (m_parsed_modules.contains(path)) {
            continue;
        }

        auto module = TRY(this->parse_file(path));
        for (auto& import : module->imports()) {
            queue.push_back(TRY(this->resolve_import_path(*import)));
        }

        m_parsed_modules.insert(path);
        program.add_module(move(module));
    }

    return move(program);
}

}