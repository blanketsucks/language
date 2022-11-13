#include "utils.h"
#include "visitor.h"
#include "lexer/lexer.h"
#include "preprocessor.h"

std::string construct_full_file_path(std::string name, std::deque<std::string> paths) {
    std::string path = "";
    for (auto& p : paths) {
        path += p + "/";
    }


    return path + name;
}

utils::filesystem::Path search_file_paths(utils::filesystem::Path path) {
    static const std::vector<std::string> SEARCH_PATHS = {"lib/"};
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
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        current_path += name;

        utils::filesystem::Path path(current_path);
        if (!path.exists()) {
            path = search_file_paths(current_path);
            if (path.isempty()) {
                ERROR(expr->start, "Could not find module '{0}'", current_path);
            }

            current_path = current_path.substr(0, current_path.size() - name.size());
            current_path += path.name;
        }

        if (!path.isdir()) {
            ERROR(expr->start, "Expected a directory, got a file");
        }

        expr->parents.pop_front();
        if (this->current_module) {
            if (this->current_module->path == current_path) {
                continue;
            }
        }

        auto module = this->scope->modules[name];
        if (!module) {
            module = utils::make_shared<Module>(name, this->format_name(name), current_path);
            module->scope = new Scope(name, ScopeType::Module);

            this->scope->children.push_back(module->scope);
            this->scope->modules[name] = module;
        }

        this->scope = module->scope;
        this->current_module = module;

        current_path += "/";
    }

    utils::filesystem::Path path(current_path + expr->name + ".pr");
    std::string path_name = path.name;

    if (!path.exists()) {
        utils::filesystem::Path dir = path.with_extension();
        if (!dir.exists()) {
            dir = search_file_paths(dir);
            if (dir.isempty()) {
                ERROR(expr->start, "Could not find module '{0}'", expr->name);
            }
        }

        if (!dir.isdir()) {
            ERROR(expr->start, "Expected a directory, got a file");
        }

        path_name = dir.name;

        path = dir.join("module.pr");
        if (!path.exists()) {
            auto module = utils::make_shared<Module>(expr->name, this->format_name(expr->name), dir.name);
            module->scope = new Scope(expr->name, ScopeType::Module);

            this->scope->children.push_back(module->scope);
            this->scope->modules[expr->name] = module;

            return nullptr;
        }

        if (!path.isfile()) {
            ERROR(expr->start, "Expected a file, got a directory");
        }
        
    }

    if (this->modules.find(path_name) != this->modules.end()) {
        this->scope->modules[expr->name] = this->modules[path_name];
        return nullptr;
    }

    std::fstream file = path.open();
    Lexer lexer(file, path.name);

    Preprocessor preprocessor(lexer.lex());
    auto tokens = preprocessor.process();

    Parser parser(tokens);
    auto ast = parser.parse();

    auto module = utils::make_shared<Module>(expr->name, this->format_name(expr->name), path_name);
    
    this->scope->modules[expr->name] = module;
    this->modules[path_name] = module;

    module->scope = this->create_scope(expr->name, ScopeType::Module);
    this->current_module = module;

    this->visit(std::move(ast));
    this->scope = scope;

    if (expr->is_wildcard) {
        
    }

    this->current_module = outer;
    return nullptr;
}