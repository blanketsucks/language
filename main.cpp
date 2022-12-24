#include "compiler.h"
#include "llvm.h"
#include "utils/filesystem.h"
#include "objects/types.h"

llvm::cl::OptionCategory category("Compiler options");

llvm::cl::opt<bool> verbose(
    "verbose", llvm::cl::desc("Enable verbose output"), llvm::cl::init(false), llvm::cl::cat(category)
);
llvm::cl::opt<bool> optimize(
    "optimize", llvm::cl::desc("Enable optimizations"), llvm::cl::init(false), llvm::cl::cat(category)
);
llvm::cl::opt<OutputFormat> format(
    "format", 
    llvm::cl::desc("Set the output format"), 
    llvm::cl::init(OutputFormat::Executable), 
    llvm::cl::values(
        clEnumValN(OutputFormat::LLVM, "llvm-ir", "Emit LLVM IR"),
        clEnumValN(OutputFormat::Bitcode, "llvm-bc", "Emit LLVM Bitcode"),
        clEnumValN(OutputFormat::Assembly, "asm", "Emit assembly code"),
        clEnumValN(OutputFormat::Object, "obj", "Emit object code"),
        clEnumValN(OutputFormat::Executable, "exe", "Emit an executable (default)"),
        clEnumValN(OutputFormat::SharedLibrary, "shared", "Emit a shared library")
    ),
    llvm::cl::cat(category)
);

llvm::cl::opt<std::string> entry("entry", llvm::cl::desc("Set an entry point for the program"), llvm::cl::init("main"), llvm::cl::cat(category));
llvm::cl::opt<std::string> output("output", llvm::cl::desc("Set an output file"), llvm::cl::Optional, llvm::cl::cat(category));
llvm::cl::list<std::string> includes("I", llvm::cl::desc("Add an include path"), llvm::cl::value_desc("path"), llvm::cl::cat(category));
llvm::cl::list<std::string> libraries("l", llvm::cl::desc("Add a library"), llvm::cl::value_desc("name"), llvm::cl::cat(category));
llvm::cl::list<std::string> files(llvm::cl::Positional, llvm::cl::desc("<files>"), llvm::cl::OneOrMore);

struct Arguments {
    std::string filename;
    std::string output;
    std::string entry;
    std::string target;

    std::vector<std::string> includes;
    Libraries libraries;

    OutputFormat format;
    bool optimize;
    bool verbose;
};


Arguments parse_arguments(int argc, char** argv) {
    Arguments args;

    llvm::cl::HideUnrelatedOptions(category);
    llvm::cl::ParseCommandLineOptions(argc, argv);

    args.filename = files.front(); 
    args.entry = entry;   
    args.format = format;
    args.optimize = optimize;
    args.verbose = verbose;
    args.includes = includes;

    args.libraries.names = libraries;

    if (output.empty()) {
        args.output = utils::filesystem::replace_extension(args.filename, "o");
        switch (args.format) {
            case OutputFormat::LLVM:
                args.output = utils::filesystem::replace_extension(args.output, "ll"); break;
            case OutputFormat::Bitcode:
                args.output = utils::filesystem::replace_extension(args.output, "bc"); break;
            case OutputFormat::Assembly:
                args.output = utils::filesystem::replace_extension(args.output, "s"); break;
            case OutputFormat::SharedLibrary:
            #if _WIN32 || _WIN64
                args.output = utils::filesystem::replace_extension(args.output, "lib"); break;
            #else
                args.output = utils::filesystem::replace_extension(args.output, "so"); break;
            #endif
            case OutputFormat::Executable:
            #if _WIN32 || _WIN64
                args.output = utils::filesystem::replace_extension(args.output, "exe"); break;
            #else
                args.output = utils::filesystem::remove_extension(args.output); break;
            #endif
            default:
                args.output = utils::filesystem::replace_extension(args.output, "o"); break;
        }
    } else {
        args.output = output;
    }

    return args;
}

int main(int argc, char** argv) {
    Arguments args = parse_arguments(argc, argv);
    Compiler::init();

    utils::filesystem::Path path(args.filename);
    if (!path.exists()) {
        Compiler::error("File not found '{0}'", args.filename); exit(1);
    }

    Compiler compiler;

    compiler.set_entry_point(args.entry);
    compiler.set_input_file(args.filename);
    compiler.set_output_file(args.output);
    compiler.set_output_format(args.format);
    compiler.set_target(args.target);
    compiler.set_verbose(args.verbose);

    compiler.set_optimization_level(args.optimize ? OptimizationLevel::Release : OptimizationLevel::Debug);
    
    for (auto& include : args.includes) {
        utils::filesystem::Path inc(include);
        if (!inc.exists()) {
            Compiler::error("Could not find include path '{0}'", include); exit(1);
        }
        
        if (!inc.isdir()) {
            Compiler::error("Include path must be a directory"); exit(1);
        }

        compiler.add_include_path(include);
    }

    compiler.set_libraries(args.libraries.names);
    compiler.set_library_paths(args.libraries.paths);

    compiler.add_library("c");

    compiler.add_include_path("lib/");
    compiler.define_preprocessor_macro("__file__", args.filename);

    compiler.compile().unwrap();

    llvm::llvm_shutdown();
    return 0;
}
