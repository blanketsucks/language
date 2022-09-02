#ifndef _COMPILER_H
#define _COMPILER_H

#include "preprocessor.h"

#include <map>
#include <vector>
#include <string>

enum class OutputFormat {
    Object,
    LLVM, // Refers to LLVM IR
    Bitcode, // Refers to LLVM Bitcode
    Assembly,
    Executable
};

static std::map<OutputFormat, const char*> OUTPUT_FORMATS_TO_STR = {
    {OutputFormat::Object, "Object"},
    {OutputFormat::LLVM, "LLVM IR"},
    {OutputFormat::Bitcode, "LLVM Bitcode"},
    {OutputFormat::Assembly, "Assembly"},
    {OutputFormat::Executable, "Executable"},
};

enum class OptimizationLevel {
    Debug,
    Release
};

struct Libraries {
    std::vector<std::string> names;
    std::vector<std::string> paths;
};

struct CompilerError {
    int code;
    std::string message;
};

class Compiler {
public:
    static void init();
    [[noreturn]] static void error(const std::string& str, ...);

    void add_library(std::string name);
    void add_library_path(std::string path);

    void add_include_path(std::string path);
    void define_preprocessor_macro(std::string name, int value);
    void define_preprocessor_macro(std::string name, std::string value);

    void set_output_format(OutputFormat format);
    void set_output_file(std::string output);

    void set_optimization_level(OptimizationLevel level);

    void set_input_file(std::string file);
    void set_entry_point(std::string entry);

    void set_linker(std::string linker);

    void add_extra_linker_option(std::string name, std::string value);
    void add_extra_linker_option(std::string name);

    std::vector<std::string> get_linker_arguments();

    void dump();

    CompilerError compile();

private:
    Libraries libraries;
    std::vector<std::string> includes;

    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel opt;

    std::string output;
    std::string input;
    std::string entry;

    std::string linker = "cc";
    std::vector<std::pair<std::string, std::string>> extras;

    std::vector<Macro> macros;
};



#endif