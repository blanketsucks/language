#pragma once

#include <quart/lexer/tokens.h>
#include <quart/filesystem.h>

#include <cstdint>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <functional>

namespace quart {

class Lexer {
public:
    virtual void reset() = 0;

    virtual char next() = 0;
    virtual char peek(uint32_t offset = 0) = 0;
    virtual char prev() = 0;
    virtual char rewind(uint32_t offset = 1) = 0;

    virtual std::string& get_line_for(const Location& location) = 0;

    Token create_token(TokenKind type, const std::string& value);
    Token create_token(TokenKind type, const Location& loc, const std::string& value);

    Location current_location();
    Span make_span();
    Span make_span(const Location& start, const Location& end);

    size_t lex_while(
        std::string& buffer,
        const std::function<bool(char)>& predicate
    );

    char escape(char current);

    bool is_valid_identifier(uint8_t current);
    uint8_t parse_unicode_identifier(std::string& buffer, uint8_t current);

    Token lex_identifier(bool accept_keywords = true);
    Token lex_string();
    Token lex_number();

    Token once();
    std::vector<Token> lex();

protected:
    uint32_t line;
    uint32_t column;
    size_t index;
    
    bool eof;
    char current;

    std::string filename;

    std::map<uint32_t, std::string> lines;
};

class MemoryLexer : public Lexer {
public:
    MemoryLexer(std::string source, const std::string& filename);
    MemoryLexer(fs::Path path);

    void reset() override;

    char next() override;
    char peek(uint32_t offset = 0) override;
    char prev() override;
    char rewind(uint32_t offset = 1) override;

    std::string& get_line_for(const Location& location) override;

private:
    std::string source;
};

class StreamLexer : public Lexer {
public:
    StreamLexer(std::ifstream& stream, const std::string& filename);
    StreamLexer(const fs::Path& path);

    void reset() override;

    char next() override;
    char peek(uint32_t offset = 0) override;
    char prev() override;
    char rewind(uint32_t offset = 1) override;

    std::string& get_line_for(const Location& location) override;

private:
    std::ifstream& stream;
};

}