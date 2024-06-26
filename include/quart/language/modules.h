#pragma once

#include <quart/filesystem.h>
#include <quart/language/symbol.h>

#include <memory>

namespace quart {

class Scope;

class Module : public Symbol {
public:

    fs::Path const& path() const { return m_path; }

    Scope* scope() const { return m_scope; }

private:
    Module(String name, fs::Path path, Scope* scope) : Symbol(move(name), Symbol::Module), m_path(move(path)), m_scope(scope) {}

    fs::Path m_path;
    Scope* m_scope;
};

// struct Module {
//     std::string name;
//     fs::Path path;

//     bool is_ready;
//     bool is_standard_library; // If the module is part of the standard library

//     Scope* scope;

//     Module(const std::string& name, const fs::Path& path, Scope* scope = nullptr);

//     std::string to_string(char sep = '.');
// };

}