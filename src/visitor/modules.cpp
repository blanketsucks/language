#include <quart/visitor.h>
#include <quart/lexer/lexer.h>

using namespace quart;

static fs::Path SEARCH_PATH = fs::Path(QUART_PATH);

std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> result;

    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        result.push_back(token);

        s.erase(0, pos + delimiter.length());
    }

    result.push_back(s);
    return result;
}

fs::Path search_file_paths(fs::Path path) {
    fs::Path p = SEARCH_PATH / path;
    if (p.exists()) {
        return p;
    }

    return fs::Path::empty();
}

RefPtr<Module> Visitor::import(const std::string& name, bool is_relative, const Span& span) {
    Scope* scope = this->scope;
    auto outer = this->current_module;

    std::string current_path;
    if (is_relative && outer) {
        current_path = outer->path.parent();
    }

    std::vector<std::string> paths = split(name, "::");

    std::string module_name = paths.back();
    paths.pop_back();

    if (this->modules.find(name) != this->modules.end()) {
        auto module = this->modules[name];
        if (!module->is_ready) {
            ERROR(span, "Could not import '{0}' because a circular dependency was detected", name);
        }

        this->scope->modules[module_name] = module;
        return module;
    }

    for (auto it = paths.begin(); it != paths.end(); ++it) {
        std::string current = *it;

        current_path += current;
        fs::Path path(current_path);

        if (!path.exists()) {
            path = search_file_paths(current_path);
            if (path.isempty()) {
                ERROR(span, "Could not find module '{0}'", name);
            }

            current_path = current_path.substr(0, current_path.size() - current.size());
            current_path += path.name;
        }

        if (!path.isdir()) {
            ERROR(span, "Expected a directory, got a file");
        }

        if (this->current_module) {
            if (this->current_module->path == current_path) {
                continue;
            }
        }

        auto module = this->scope->modules[current];
        if (!module) {
            if (this->modules.find(current_path) != this->modules.end()) {
                module = this->modules[current_path];
            } else {
                module = make_ref<Module>(current, path);
                
                module->scope = Scope::create(current, ScopeType::Module);
                this->scope->add_child(module->scope);
            }

            this->scope->modules[current] = module;
        }

        this->scope = module->scope;
        current_path += "/";
    }

    fs::Path path = fs::Path(current_path + module_name + FILE_EXTENSION);
    std::string path_name = path;

    if (!path.exists()) {
        fs::Path dir = path.with_extension();
        if (!dir.exists()) {
            dir = search_file_paths(dir);
            if (dir.isempty()) {
                ERROR(span, "Could not find module '{0}'", name);
            }
        }

        if (!dir.isdir()) {
            ERROR(span, "Expected a directory, got a file");
        }

        path_name = dir.name;

        path = dir.join("module.qr");
        if (!path.exists()) {
            auto module = make_ref<Module>(module_name, dir);
            module->scope = Scope::create(module_name, ScopeType::Module);

            this->scope->add_child(module->scope);
            this->scope->modules[module_name] = module;

            this->scope = scope;
            this->current_module = outer;

            return module;
        }

        if (!path.isfile()) {
            ERROR(span, "Expected a file, got a directory");
        }
    }

    if (path_name == this->name) {
        ERROR(span, "Could not import '{0}' because a circular dependency was detected", name);
    }

    MemoryLexer lexer(path);
    auto tokens = lexer.lex();

    Parser parser(tokens);
    auto ast = parser.parse();

    auto module = make_ref<Module>(module_name, path);
    module->scope = Scope::create(module_name, ScopeType::Module);

    this->scope->add_child(module->scope);

    scope->modules[module_name] = module;
    this->modules[name] = module;

    this->scope = module->scope;
    this->current_module = module;

    this->visit(std::move(ast));
    this->scope = scope;

    module->is_ready = true;
    this->current_module = outer;

    return module;
}

Value Visitor::visit(ast::ModuleExpr* expr) {
    if (this->scope->modules.find(expr->name) != this->scope->modules.end()) {
        ERROR(expr->span, "Module '{0}' already exists", expr->name);
    }

    auto outer = this->current_module;

    auto module = make_ref<Module>(expr->name, expr->name);
    this->scope->modules[expr->name] = module;

    module->scope = Scope::create(expr->name, ScopeType::Module);
    this->scope->add_child(module->scope);

    Scope* prev = this->scope;

    this->current_module = module;
    this->push_scope(module->scope);

    this->visit(std::move(expr->body));

    this->current_module = outer;
    this->scope = prev;

    return nullptr;
}

Value Visitor::visit(ast::ImportExpr* expr) {
    this->import(expr->name, expr->is_relative, expr->span);
    return nullptr;
}