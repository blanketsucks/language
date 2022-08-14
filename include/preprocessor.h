#ifndef _PREPROCESSOR_H
#define _PREPROCESSOR_H

#include "lexer/tokens.h"

enum class IncludeState {
    Initialized,
    Processed
};

struct Include {
    std::string path;
    IncludeState state;
};

struct Macro {
    std::string name;
    std::vector<std::string> args;
    std::vector<Token> body;

    Macro() {}
    Macro(std::string name, std::vector<std::string> args, std::vector<Token> body) : name(name), args(args), body(body) {}

    bool is_callable() { return this->args.size() > 0; }

    std::vector<Token> expand();
    std::vector<Token> expand(std::map<std::string, Token>& args);
};

class Preprocessor {
public:
    Preprocessor(std::vector<Token> tokens, std::vector<std::string> include_paths = {});

    void next();
    std::vector<Token> skip_until(TokenKind type, std::vector<std::string> values);

    std::vector<Token> process();

    Macro parse_macro_definition();
    void parse_include(std::string path);

    std::fstream search_include_paths(std::string path);

    bool is_macro(std::string name);

    void update(std::vector<Token> tokens);
    void update(std::map<std::string, Macro> macros);

    void define(std::string name);
    std::vector<Token> expand(Macro macro, bool return_tokens = false);

    static std::vector<Token> run(std::vector<Token> tokens);

    Token current;
    std::vector<Token> tokens;
    uint32_t index;
    std::vector<std::string> include_paths;
    std::vector<Token> processed;
    std::map<std::string, Macro> macros;
    std::map<std::string, Include> includes;
};

// Lexer lexer(text, filename);
// std::vector<Token> tokens = Preprocessor::run(lexer.lex());

#endif