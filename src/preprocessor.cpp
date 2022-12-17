#include "preprocessor.h"

#include "utils/filesystem.h"
#include "lexer/lexer.h"
#include "utils/log.h"

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
        if (token == TokenKind::Identifier) {
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

std::vector<Token> Preprocessor::skip_until(TokenKind type, std::vector<std::string> values) {
    std::vector<Token> tokens;
    while (this->current != TokenKind::EOS && !this->current.match(type, values)) {
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
    while (this->current != TokenKind::EOS) {
        if (this->current == TokenKind::Keyword) {
            if (this->current.value == "$define") {
                this->next();
                this->parse_macro_definition();
            } else if (this->current.value == "$undef") {
                this->next();
                std::string name = this->current.value;

                if (!this->is_macro(name)) {
                    ERROR(this->current.start, "Undefined macro '{0}'", name); exit(1);
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
            } else if (this->current.value == "$ifdef") {
                this->if_directive_location = this->current.start;
                this->next();

                std::string name = this->current.value;
                this->has_if_directive = true;
                if (!this->is_macro(name)) {
                    this->skip_until(TokenKind::Keyword, {"$else", "$endif"}); continue;
                }

                this->should_close_if_directive = true;
                this->next();
            } else if (this->current.value == "$ifndef") {
                this->if_directive_location = this->current.start;
                this->next();

                std::string name = this->current.value;
                this->has_if_directive = true;
                if (this->is_macro(name)) {
                    this->skip_until(TokenKind::Keyword, {"$else", "$endif"}); continue;
                }

                this->should_close_if_directive = true;
                this->next();
            } else if (this->current.value == "$else") {
                if (!this->has_if_directive) {
                    ERROR(this->current.start, "Unexpected '$else'");
                }

                this->next();
                if (this->should_close_if_directive) {
                    this->skip_until(TokenKind::Keyword, {"$endif"}); continue;
                }
            } else if (this->current.value == "$endif") {
                if (!this->has_if_directive) {
                    ERROR(this->current.start, "Unexpected '$endif'");
                }

                this->next();

                this->should_close_if_directive = false;
                this->has_if_directive = false;
            }
        } else if (this->current == TokenKind::Identifier) {
            if (this->is_macro(this->current.value)) {
                Macro macro = this->macros[this->current.value];
                this->expand(macro);
            }
        }

        if (this->current != TokenKind::Newline) {
            this->processed.push_back(this->current);
        }

        this->next();
    }

    if (this->has_if_directive) {
        ERROR(this->if_directive_location, "Unterminated if directive");
    }
    
    this->processed.push_back(this->current);
    return this->processed;
}

Macro Preprocessor::parse_macro_definition() {
    if (this->current != TokenKind::Identifier) {
        ERROR(this->current.start, "Expected identifier");
    }
 
    std::string name = this->current.value;
    this->next();

    std::vector<std::string> args;
    if (this->current == TokenKind::LParen) {
        this->next();

        while (this->current != TokenKind::RParen) {
            if (this->current != TokenKind::Identifier) {
                ERROR(this->current.start, "Expected identifier");
            }

            args.push_back(this->current.value);
            this->next();

            if (this->current == TokenKind::Comma) {
                this->next();
            }
        }

        this->next();
    }

    std::vector<Token> body;
    while (this->current != TokenKind::Newline && this->current != TokenKind::EOS) {
        body.push_back(this->current);
        this->next();
    }

    Macro macro(name, args, body);
    this->macros[name] = macro;

    return macro;
}

std::fstream Preprocessor::search_include_paths(std::string filename) {
    utils::filesystem::Path path(filename);
    if (path.exists()) {
        if (!path.isfile()) {
            ERROR(this->current.start, "'{0}' is not a file", filename);
        }

        return path.open();
    }

    for (auto& search : this->include_paths) {
        path = utils::filesystem::Path(search).join(filename);
        if (path.exists()) {
            if (!path.isfile()) {
                ERROR(this->current.start, "'{0}' is not a file", path.name);
            }

            return path.open();
        }
    }

    ERROR(this->current.start, "Could not find file '{0}'", filename);
}

void Preprocessor::parse_include(std::string path) {
    std::fstream file = this->search_include_paths(path);
    if (this->includes.find(path) != this->includes.end()) {
        Include inc = this->includes[path];
        if (inc.state != IncludeState::Processed) {
            ERROR(this->current.start, "Circluar dependency detected in include path '{0}'", path);
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
    while (tokens.back() == TokenKind::EOS) {
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

void Preprocessor::undef(std::string name) {
    this->macros.erase(name);
}

std::vector<Token> Preprocessor::expand(Macro macro, bool return_tokens) {
    std::vector<Token> tokens;
    if (macro.is_callable()) {
        this->next();
        if (this->current != TokenKind::LParen) {
            ERROR(this->current.start, "Expected '('");
        }

        this->next();

        std::map<std::string, Token> args;
        size_t index = 0;
        while (this->current != TokenKind::RParen) {
            if (index >= macro.args.size()) {
                ERROR(this->current.start, "Too many arguments passed to macro call", false);

                std::string message = FORMAT(
                    "'{0}' macro expects {1} arguments but got {2}", macro.name, macro.args.size(), index + 1
                );
                utils::note(this->current.start, message);

                exit(1);
            }

            args[macro.args[index]] = this->current;
            this->next();

            if (this->current != TokenKind::Comma) {
                break;
            }

            this->next();
            index++;
        }

        if (this->current != TokenKind::RParen) {
            ERROR(this->current.start, "Expected ')'");
        }

        if (index < macro.args.size() - 1) {
            ERROR(this->current.start, "Too few arguments passed to macro call", false);
            NOTE(this->current.start, "'{s}' macro expects {i} arguments but got {i}", macro.name, macro.args.size(), index + 1);
            
            exit(1);
        }


        this->next();
        tokens = macro.expand(args);
    } else {
        this->next();
        tokens = macro.expand();
    }

    std::vector<Token> new_tokens;
    for (auto token : tokens) {
        if (this->is_macro(this->current.value)) {
            Macro macro = this->macros[this->current.value];
            auto expanded = this->expand(macro, true);

            new_tokens.insert(new_tokens.end(), expanded.begin(), expanded.end());
        } else {
            new_tokens.push_back(token);
        }
    }

    if (return_tokens) {
        return new_tokens;
    }

    this->update(new_tokens);
    return {};
}

std::vector<Token> Preprocessor::run(std::vector<Token> tokens) {
    Preprocessor preprocessor(tokens);
    return preprocessor.process();
}