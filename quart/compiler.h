#pragma once

#include <quart/filesystem.h>
#include <quart/common.h>

#include <set>
#include <chrono>

#include <llvm/Target/TargetMachine.h>

namespace quart {

class State;

enum class OutputFormat : u8 {
    Object,
    LLVM,    // Refers to LLVM IR
    Bitcode, // Refers to LLVM Bitcode
    Assembly,
    Executable,
    SharedLibrary
};

static const std::map<OutputFormat, StringView> OUTPUT_FORMATS_TO_STR = {
    {OutputFormat::Object, "Object"},
    {OutputFormat::LLVM, "LLVM IR"},
    {OutputFormat::Bitcode, "LLVM Bitcode"},
    {OutputFormat::Assembly, "Assembly"},
    {OutputFormat::Executable, "Executable"},
    {OutputFormat::SharedLibrary, "Shared Library"}
};

static const std::map<OutputFormat, StringView> OUTPUT_FORMATS_TO_EXT = {
    {OutputFormat::Object, "o"},
    {OutputFormat::LLVM, "ll"},
    {OutputFormat::Bitcode, "bc"},
    {OutputFormat::Assembly, "s"},
    {OutputFormat::Executable, {}},
#if _WIN32 || _WIN64
    {OutputFormat::SharedLibrary, "lib"}
#else
    {OutputFormat::SharedLibrary, "so"}
#endif
};

enum class OptimizationLevel : u8 {
    O0,
    O1,
    O2,
    O3,
    Os,
    Oz
};

enum class MangleStyle : u8 {
    Full,
    Minimal,
    None
};

struct OptimizationOptions {
    OptimizationLevel level = OptimizationLevel::O2;
    bool dead_code_elimination = true;

    MangleStyle mangle_style = MangleStyle::Full; // Not really an optimization, but it's here for now
};

struct CompilerOptions {
    using Extra = std::pair<String, String>;

    fs::Path file;
    String output;

    String entry;
    String target;

    std::set<String> library_names;
    std::set<String> library_paths;

    Vector<String> imports;

    String linker = "cc";

    OutputFormat format = OutputFormat::Executable;
    OptimizationOptions opts;

    bool verbose = false;
    bool no_libc = false;

    Vector<String> object_files;
    Vector<Extra> extras;

    bool has_target() const { return !this->target.empty(); }
    
    void add_library_name(const String& name) {
        library_names.insert(name);
    }

    void add_library_path(const String& path) {
        library_paths.insert(path);
    }
};

class Compiler {
public:
    using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;

    static TimePoint now();
    static double duration(TimePoint start, TimePoint end);
    static void debug(const char* message, TimePoint start);

    static void init();
    static void shutdown();

    Compiler(CompilerOptions options) : m_options(move(options)) {}
    
    CompilerOptions const& options() { return m_options; }

    void add_library(const String& name) {
        m_options.add_library_name(name);
    }

    void add_library_path(const String& path) {
        m_options.add_library_path(path);
    }

    void set_libraries(std::set<String> names) {
        m_options.library_names = move(names);
    }

    void set_library_paths(std::set<String> paths) {
        m_options.library_paths = move(paths);
    }

    void add_import_path(const String& path) {
        m_options.imports.push_back(path);
    }

    void set_output_format(OutputFormat format) {
        m_options.format = format;
    }

    void set_output_file(const String& output) {
        m_options.output = output;
    }

    void set_optimization_level(OptimizationLevel level) {
        m_options.opts.level = level;
    }

    void set_optimization_options(const OptimizationOptions& optimization) {
        m_options.opts = optimization;
    }

    void set_input_file(const fs::Path& input) {
        m_options.file = input;
    }

    void set_entry_point(const String& entry) {
        m_options.entry = entry;
    }

    void set_target(const String& target) {
        m_options.target = target;
    }

    void set_verbose(bool verbose) {
        m_options.verbose = verbose;
    }

    void set_linker(const String& linker) {
        m_options.linker = linker;
    }

    void add_object_file(const String& file) {
        m_options.object_files.push_back(file);
    }

    void add_extra_linker_option(const String& name, const String& value) {
        m_options.extras.emplace_back(name, value);
    }

    void add_extra_linker_option(const String& name) {
        m_options.extras.emplace_back(name, "");
    }

    Vector<String> get_linker_arguments() const;

    void dump() const;

    int compile() const;

private:
    void run_bytecode_passes(State&) const;

    CompilerOptions m_options;
};

}