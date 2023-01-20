#include "visitor.h"
#include "lexer/lexer.h"
#include "preprocessor.h"

static const std::vector<std::string> SEARCH_PATHS = {"lib/"};

utils::filesystem::Path search_file_paths(utils::filesystem::Path path) {
    for (auto& p : SEARCH_PATHS) {
        auto full_path = utils::filesystem::Path(p + path.name);
        if (full_path.exists()) {
            return full_path;
        }
    }
    
    return utils::filesystem::Path::empty();
}

Value Visitor::visit(ast::ImportExpr* expr) {
    Scope* scope = this->scope;
    auto outer = this->current_module;

    std::string current_path;
    if (expr->is_relative && outer) {
        current_path = outer->path.parent();
    }

    auto names = utils::split(expr->name, "::");

    std::string module_name = names.back();
    names.pop_back();

    if (this->modules.find(expr->name) != this->modules.end()) {
        auto module = this->modules[expr->name];
        if (!module->is_ready) {
            ERROR(expr->span, "Could not import '{0}' because a circular dependency was detected", expr->name);
        }

        this->scope->modules[module_name] = module;

        this->scope = scope;
        this->current_module = outer;

        return nullptr;
    }

    for (auto it = names.begin(); it != names.end(); ++it) {
        std::string current = *it;

        current_path += current;
        utils::filesystem::Path path(current_path);

        if (!path.exists()) {
            path = search_file_paths(current_path);
            if (path.isempty()) {
                ERROR(expr->span, "Could not find module '{0}'", expr->name);
            }

            current_path = current_path.substr(0, current_path.size() - current.size());
            current_path += path.name;
        }

        if (!path.isdir()) {
            ERROR(expr->span, "Expected a directory, got a file");
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
                module = utils::make_ref<Module>(current, current_path);
                module->scope = new Scope(current, ScopeType::Module);

                this->scope->children.push_back(module->scope);
            }

            this->scope->modules[current] = module;
        }

        this->scope = module->scope;
        current_path += "/";
    }

    utils::filesystem::Path path(current_path + module_name + FILE_EXTENSION);
    std::string path_name = path.name;

    if (!path.exists()) {
        utils::filesystem::Path dir = path.with_extension();
        if (!dir.exists()) {
            dir = search_file_paths(dir);
            if (dir.isempty()) {
                ERROR(expr->span, "Could not find module '{0}'", expr->name);
            }
        }

        if (!dir.isdir()) {
            ERROR(expr->span, "Expected a directory, got a file");
        }

        path_name = dir.name;

        path = dir.join("module.qr");
        if (!path.exists()) {
            auto module = utils::make_ref<Module>(module_name, dir);
            module->scope = new Scope(module_name, ScopeType::Module);

            this->scope->children.push_back(module->scope);
            this->scope->modules[module_name] = module;

            this->scope = scope;
            this->current_module = outer;

            return nullptr;
        }

        if (!path.isfile()) {
            ERROR(expr->span, "Expected a file, got a directory");
        }
        
    }
    
    if (path_name == this->name) {
        ERROR(expr->span, "Could not import '{0}' because a circular dependency was detected", expr->name);
    }

    Lexer lexer(path);

    Preprocessor preprocessor(lexer.lex());
    preprocessor.define("__file__", path.filename());

    auto tokens = preprocessor.process();

    Parser parser(tokens);
    auto ast = parser.parse();

    auto module = utils::make_ref<Module>(module_name, path);
    
    this->scope->modules[module_name] = module;
    this->modules[expr->name] = module;

    module->scope = this->create_scope(module_name, ScopeType::Module);
    this->current_module = module;

    this->visit(std::move(ast));
    this->scope = scope;

    if (expr->is_wildcard) {
        
    }

    module->is_ready = true;
    this->current_module = outer;

    return nullptr;
}