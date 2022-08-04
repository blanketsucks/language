#include "preprocessor.h"

#include <sys/stat.h>
#include <fstream>

#include "lexer.h"
#include "utils.h"

bool file_exists(const std::string& name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

std::vector<Token> Macro::expand()  {
    std::vector<Token> tokens;
    for (auto& token : this->body) {
        tokens.push_back(token);
    }

    return tokens;
}

std::vector<Token> Macro::expand(std::map<std::string, Token>& args)  {
    std::vector<Token> tokens;
    for (auto& token : this->body) {
        if (token == TokenType::Identifier) {
            if (args.find(token.value) != args.end()) {
                tokens.push_back(args[token.value]);
            } else {
                tokens.push_back(token);
            }
        } else {
            tokens.push_back(token);
        }
    }

    return tokens;
}

Preprocessor::Preprocessor(std::vector<Token> tokens, std::vector<std::string> include_paths) {
    this->tokens = tokens;
    this->index = 0;
    this->current = this->tokens.front();
    this->include_paths = include_paths;
}

void Preprocessor::next() {
    this->index++;
    if (this->index >= this->tokens.size()) {
        this->current = this->tokens.back();
    } else {
        this->current = this->tokens[this->index];
    }
}

std::vector<Token> Preprocessor::skip_until(TokenType type, std::vector<std::string> values) {
    auto in = [](std::vector<std::string> values, std::string value) {
        return std::find(values.begin(), values.end(), value) == values.end();
    };

    std::vector<Token> tokens;
    while (this->current != TokenType::EOS && this->current != type && in(values, this->current.value)) {
        tokens.push_back(this->current);
        this->next();
    }

    return tokens;
}

bool Preprocessor::is_macro(std::string name) {
    return this->macros.find(name) != this->macros.end();
}

void Preprocessor::update(std::vector<Token> tokens) {
    this->processed.insert(this->processed.end(), tokens.begin(), tokens.end());
}

void Preprocessor::update(std::map<std::string, Macro> macros) {
    this->macros.insert(macros.begin(), macros.end());
}

std::vector<Token> Preprocessor::process() {
    while (this->current != TokenType::EOS) {
        if (this->current == TokenType::Keyword) {
            if (this->current.value == "$define") {
                this->next();
                this->parse_macro_definition();
            } else if (this->current.value == "$undef") {
                this->next();
                std::string name = this->current.value;

                if (!this->is_macro(name)) {
                    ERROR(this->current.start, "Undefined macro '{s}'", name); exit(1);
                }

                this->macros.erase(name);
                this->next();
            } else if (this->current.value == "$error") {
                this->next();
                utils::error(this->current.start, this->current.value);
            } else if (this->current.value == "$include") {
                this->next();
                std::string path = this->current.value;

                this->parse_include(path);
            }
        } else if (this->current == TokenType::Identifier) {
            if (this->is_macro(this->current.value)) {
                Macro macro = this->macros[this->current.value];
                this->expand(macro);
            }
        }

        if (this->current != TokenType::Newline) {
            this->processed.push_back(this->current);
        }

        this->next();
    }
    
    this->processed.push_back(this->current);
    return this->processed;
}

Macro Preprocessor::parse_macro_definition() {
    if (this->current != TokenType::Identifier) {
        ERROR(this->current.start, "Expected identifier"); exit(1);
    }
 
    std::string name = this->current.value;
    this->next();

    std::vector<std::string> args;
    if (this->current == TokenType::LParen) {
        this->next();

        while (this->current != TokenType::RParen) {
            if (this->current != TokenType::Identifier) {
                ERROR(this->current.start, "Expected identifier"); exit(1);
            }

            args.push_back(this->current.value);
            this->next();

            if (this->current == TokenType::Comma) {
                this->next();
            }
        }

        this->next();
    }

    std::vector<Token> body;
    while (this->current != TokenType::Newline && this->current != TokenType::EOS) {
        body.push_back(this->current);
        this->next();
    }

    Macro macro(name, args, body);
    this->macros[name] = macro;

    return macro;
}

std::ifstream Preprocessor::search_include_paths(std::string path) {
    if (file_exists(path)) {
        return std::ifstream(path);
    }

    for (auto& search : this->include_paths) {
        std::string full_path = search + path;
        if (file_exists(full_path)) {
            return std::ifstream(full_path);
        }
    }

    ERROR(this->current.start, "Could not find include file '{s}'", path); exit(1);
}

void Preprocessor::parse_include(std::string path) {
    std::ifstream file = this->search_include_paths(path);
    if (this->includes.find(path) != this->includes.end()) {
        Include inc = this->includes[path];
        if (inc.state != IncludeState::Processed) {
            ERROR(this->current.start, "Circluar dependency detected in include path '{s}'", path);
        }

        this->next();
        return;
    }

    this->next();
    Lexer lexer(file, path);

    Include include = {path, IncludeState::Initialized};
    this->includes[path] = include;

    Preprocessor preprocessor(lexer.lex(), this->include_paths);
    preprocessor.includes = this->includes;

    std::vector<Token> tokens = preprocessor.process();
    while (tokens.back() == TokenType::EOS) {
        tokens.pop_back();
    }

    include.state = IncludeState::Processed;

    this->update(tokens);
    this->update(preprocessor.macros);

    if (file.is_open()) {
        file.close();
    }
}

void Preprocessor::define(std::string name) {
    this->macros[name] = Macro(name, {}, {});
}

std::vector<Token> Preprocessor::expand(Macro macro, bool return_tokens) {
    std::vector<Token> tokens;
    if (macro.is_callable()) {
        this->next();
        if (this->current != TokenType::LParen) {
            ERROR(this->current.start, "Expected '('"); exit(1);
        }

        this->next();

        std::map<std::string, Token> args;
        size_t index = 0;
        while (this->current != TokenType::RParen) {
            if (index >= macro.args.size()) {
                utils::error(this->current.start, "Too many arguments passed to macro call", false);

                std::string message = utils::fmt::format(
                    "'{s}' macro expects {i} arguments but got {i}", macro.name, macro.args.size(), index + 1
                );
                utils::note(this->current.start, message);

                exit(1);
            }

            args[macro.args[index]] = this->current;
            this->next();

            if (this->current != TokenType::Comma) {
                break;
            }

            this->next();
            index++;
        }

        if (this->current != TokenType::RParen) {
            ERROR(this->current.start, "Expected ')'"); exit(1);
        }

        if (index < macro.args.size() - 1) {
            utils::error(this->current.start, "Too few arguments passed to macro call", false);
            NOTE(this->current.start, "'{s}' macro expects {i} arguments but got {i}", macro.name, macro.args.size(), index + 1);
            
            exit(1);
        }


        this->next();
        tokens = macro.expand(args);
    } else {
        this->next();
        tokens = macro.expand();
    }

    for (auto token : tokens) {
        if (this->is_macro(this->current.value)) {
            Macro macro = this->macros[this->current.value];
            auto expanded = this->expand(macro, true);

            tokens.insert(tokens.end(), expanded.begin(), expanded.end());
        }
    }

    if (return_tokens) {
        return tokens;
    }

    this->update(tokens);
    return {};
}

std::vector<Token> Preprocessor::run(std::vector<Token> tokens) {
    Preprocessor preprocessor(tokens);
    return preprocessor.process();
}