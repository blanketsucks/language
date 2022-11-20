#ifndef _COMPILER_H
#define _COMPILER_H

#include "preprocessor.h"
#include "utils.h"

#include "llvm/Support/FormatVariadic.h"

#include <map>
#include <vector>
#include <string>
#include <chrono>

enum class OutputFormat {
    Object,
    LLVM, // Refers to LLVM IR
    Bitcode, // Refers to LLVM Bitcode
    Assembly,
    Executable,
    SharedLibrary
};

static std::map<OutputFormat, const char*> OUTPUT_FORMATS_TO_STR = {
    {OutputFormat::Object, "Object"},
    {OutputFormat::LLVM, "LLVM IR"},
    {OutputFormat::Bitcode, "LLVM Bitcode"},
    {OutputFormat::Assembly, "Assembly"},
    {OutputFormat::Executable, "Executable"},
    {OutputFormat::SharedLibrary, "Shared Library"}
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
    uint32_t code;
    std::string message;

    void unwrap();
};

class Compiler {
public:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static TimePoint now();
    static double duration(TimePoint start, TimePoint end);
    static void log_duration(const char* message, TimePoint start);

    static void init();

    template<typename... Ts> static void error(const std::string& str, Ts&&... values) {
        std::string fmt = llvm::formatv(str.c_str(), std::forward<Ts>(values)...);
        std::string message = FORMAT(
            "{0} {1} {2}", utils::color(WHITE, "proton:"), utils::color(RED, "error:"), fmt
        );

        std::cout << message << std::endl;
    }

    void add_library(std::string name);
    void add_library_path(std::string path);

    void set_libraries(std::vector<std::string> names);
    void set_library_paths(std::vector<std::string> paths);

    void add_include_path(std::string path);
    void define_preprocessor_macro(std::string name, int value);
    void define_preprocessor_macro(std::string name, std::string value);

    void set_output_format(OutputFormat format);
    void set_output_file(std::string output);

    void set_optimization_level(OptimizationLevel level);

    void set_input_file(std::string file);
    void set_entry_point(std::string entry);

    void set_target(std::string target);

    void set_verbose(bool verbose);

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

    bool verbose = false;

    std::string output;
    std::string input;
    std::string entry;
    std::string target;

    std::string linker = "cc";
    std::vector<std::pair<std::string, std::string>> extras;

    std::vector<Macro> macros;
};


#endif