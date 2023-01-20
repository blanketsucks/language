#ifndef _COMPILER_H
#define _COMPILER_H

#include "preprocessor.h"
#include "utils/log.h"
#include "utils/filesystem.h"
#include "visitor.h"

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

    bool empty() const { return this->names.empty() || this->paths.empty(); }
};

struct CompilerError {
    uint32_t code;
    std::string message;

    CompilerError(uint32_t code, std::string message);
    static CompilerError success();

    void unwrap();
};

struct CompilerOptions {
    using Extra = std::pair<std::string, std::string>;

    utils::filesystem::Path input;
    std::string output;
    std::string entry;
    std::string target;

    Libraries libs;
    std::vector<std::string> includes;

    std::string linker = "cc";

    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel optimization = OptimizationLevel::Debug;
    OptimizationOptions opts;

    bool verbose = false;

    std::vector<std::string> object_files;
    std::vector<Extra> extras;

    bool has_target() const { return !this->target.empty(); }
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
            "{0} {1} {2}", utils::color(WHITE, "quart:"), utils::color(RED, "error:"), fmt
        );

        std::cout << message << std::endl;
    }

    Compiler(const CompilerOptions& options) : options(options) {}
    
    CompilerOptions& get_options() { return this->options; }

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
    void set_optimization_options(OptimizationOptions opts);

    void set_input_file(const utils::filesystem::Path& input);
    void set_entry_point(std::string entry);

    void set_target(std::string target);

    void set_verbose(bool verbose);

    void set_linker(std::string linker);

    void add_object_file(std::string file);

    void add_extra_linker_option(std::string name, std::string value);
    void add_extra_linker_option(std::string name);

    std::vector<std::string> get_linker_arguments();

    void dump();

    CompilerError compile();

private:
    CompilerOptions options;
    std::vector<Macro> macros;
};


#endif