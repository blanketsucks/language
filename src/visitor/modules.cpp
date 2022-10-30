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
    Module* outer = this->current_module;

    std::string full_path = construct_full_file_path(expr->name, expr->parents);

    std::string current_path;
    while (!expr->parents.empty()) {
        std::string name = expr->parents.front();
        current_path += name;

        utils::filesystem::Path path(current_path);
        if (!path.exists()) {
            ERROR(expr->start, "Could not find module '{0}'", name);
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

        Module* module = this->scope->modules[name];
        if (!module) {
            module = new Module(name, this->format_name(name), current_path);
            module->scope = new Scope(name, ScopeType::Module);

            this->scope->modules[name] = module;
        }

        this->scope = module->scope;
        this->current_module = module;

        current_path += "/";
    }

    utils::filesystem::Path path(full_path + ".pr");
    std::string path_name = path.name;

    if (!path.exists()) {
        utils::filesystem::Path dir = path.with_extension();
        if (!dir.exists()) {
            dir = search_file_paths(full_path);
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
            Module* module = new Module(expr->name, this->format_name(expr->name), dir.name);
            module->scope = new Scope(expr->name, ScopeType::Module);

            this->scope->modules[expr->name] = module;
            return nullptr;
        }

        if (!path.isfile()) {
            ERROR(expr->start, "Expected a file, got a directory");
        }
        
    }

    std::fstream file = path.open();
    Lexer lexer(file, path.name);

    Preprocessor preprocessor(lexer.lex());
    auto tokens = preprocessor.process();

    Parser parser(tokens);
    auto ast = parser.parse();

    Module* module = new Module(expr->name, this->format_name(expr->name), path_name);
    this->scope->modules[expr->name] = module;

    module->scope = this->create_scope(expr->name, ScopeType::Module);\
    this->current_module = module;

    this->visit(std::move(ast));

    this->scope = scope;

    if (expr->is_wildcard) {
        TODO("Wildcard imports");
    }

    this->current_module = outer;
    return nullptr;
}