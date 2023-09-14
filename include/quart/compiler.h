#pragma once

#include <quart/logging.h>
#include <quart/filesystem.h>

#include <set>
#include <chrono>

namespace quart {

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

static std::map<OutputFormat, std::string> OUTPUT_FORMATS_TO_EXT = {
    {OutputFormat::Object, "o"},
    {OutputFormat::LLVM, "ll"},
    {OutputFormat::Bitcode, "bc"},
    {OutputFormat::Assembly, "s"},
    {OutputFormat::Executable, ""},
#if _WIN32 || _WIN64
    {OutputFormat::SharedLibrary, "lib"}
#else
    {OutputFormat::SharedLibrary, "so"}
#endif
};


enum class OptimizationLevel {
    Debug,
    Release
};

enum class MangleStyle {
    Full,
    Minimal,
    None
};

struct OptimizationOptions {
    bool enable = true;
    bool dead_code_elimination = true;

    MangleStyle mangle_style = MangleStyle::Full; // Not really an optimization, but it's here for now
};

struct Libraries {
    std::set<std::string> names;
    std::set<std::string> paths;

    bool empty() const { return this->names.empty() || this->paths.empty(); }
};

struct CompilerError {
    uint32_t code;
    std::string message;

    CompilerError(uint32_t code, const std::string& message);
    static CompilerError success();

    void unwrap();
};

struct CompilerOptions {
    using Extra = std::pair<std::string, std::string>;

    fs::Path input;
    std::string output;
    std::string entry;
    std::string target;

    Libraries libs;
    std::vector<std::string> imports;

    std::string linker = "cc";

    OutputFormat format = OutputFormat::Executable;
    OptimizationLevel optimization = OptimizationLevel::Debug;
    OptimizationOptions opts;

    bool verbose = false;
    bool standalone = false;

    std::vector<std::string> object_files;
    std::vector<Extra> extras;

    bool has_target() const { return !this->target.empty(); }
    void add_library(const std::string& name, bool is_path = false) {
        if (is_path) {
            this->libs.paths.insert(name);
        } else {
            this->libs.names.insert(name);
        }
    }
};

class Compiler {
public:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static TimePoint now();
    static double duration(TimePoint start, TimePoint end);
    static void log_duration(const char* message, TimePoint start);

    static void init();
    static void shutdown();

    template<typename... Ts> static void error(const std::string& str, Ts&&... values) {
        std::string fmt = llvm::formatv(str.c_str(), std::forward<Ts>(values)...);
        std::string message = FORMAT(
            "{0} {1} {2}", logging::color(WHITE, "quart:"), logging::color(RED, "error:"), fmt
        );

        std::cout << message << std::endl;
    }

    Compiler(const CompilerOptions& options) : options(options) {}
    
    CompilerOptions& get_options() { return this->options; }

    void add_library(const std::string& name);
    void add_library_path(const std::string& path);

    void set_libraries(std::set<std::string> names);
    void set_library_paths(std::set<std::string> paths);

    void add_import_path(const std::string& path);

    void set_output_format(OutputFormat format);
    void set_output_file(const std::string& output);

    void set_optimization_level(OptimizationLevel level);
    void set_optimization_options(OptimizationOptions opts);

    void set_input_file(const fs::Path& input);
    void set_entry_point(const std::string& entry);

    void set_target(const std::string& target);

    void set_verbose(bool verbose);

    void set_linker(const std::string& linker);

    void add_object_file(const std::string& file);

    void add_extra_linker_option(const std::string& name, const std::string& value);
    void add_extra_linker_option(const std::string& name);

    std::vector<std::string> get_linker_arguments();

    void dump();

    CompilerError compile();
    int jit(int argc, char** argv);

private:
    CompilerOptions options;
};

}