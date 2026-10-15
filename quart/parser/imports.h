#pragma once

#include <quart/common.h>
#include <quart/errors.h>
#include <quart/parser/ast.h>

#include <filesystem>

namespace quart {

class Program {
public:
    void add_module(OwnPtr<ast::ParseModule> module) { m_modules.push_back(move(module)); }

    Vector<OwnPtr<ast::ParseModule>> const& modules() const { return m_modules; }
    ast::ParseModule const& entry() const { return *m_modules.front(); }

private:
    Vector<OwnPtr<ast::ParseModule>> m_modules;
};

class ImportLoader {
public:
    ImportLoader(State& state, const Vector<fs::Path>& import_search_paths = {});

    HashMap<fs::Path, RefPtr<Module>> const& modules() const { return m_modules; }

    Set<fs::Path>& parsed_modules() { return m_parsed_modules; }
    Set<fs::Path> const& parsed_modules() const { return m_parsed_modules; }

    ErrorOr<fs::Path> resolve_import_path(ast::ImportExpr const& import);
    fs::Path search_import_paths(const fs::Path& path) const;
    
    RefPtr<Module> lookup(const fs::Path&) const;

    ErrorOr<OwnPtr<ast::ParseModule>> parse_file(const fs::Path&);

    ErrorOr<Program> load_imports(ast::ParseModule const& module);

private:
    using PathKey = Pair<String, Vector<String>>;

    PathKey make_key(const Path&);

    HashMap<PathKey, fs::Path> m_paths;

    HashMap<fs::Path, RefPtr<Module>> m_modules;
    Set<fs::Path> m_parsed_modules;

    Vector<fs::Path> m_import_search_paths;

    State* m_state;
};

}