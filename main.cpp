#include "compiler.h"

#include "llvm/Support/CommandLine.h"

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

llvm::cl::opt<MangleStyle> mangling(
    "mangle-style", 
    llvm::cl::desc("Set the mangling style"), 
    llvm::cl::init(MangleStyle::Full), 
    llvm::cl::values(
        clEnumValN(MangleStyle::Full, "full", "Use the default mangling style"),
        clEnumValN(MangleStyle::Minimal, "minimal", "Use a minimal mangling style"),
        clEnumValN(MangleStyle::None, "none", "Do not mangle names")
    ),
    llvm::cl::cat(category)
);

llvm::cl::opt<std::string> entry("entry", llvm::cl::desc("Set an entry point for the program"), llvm::cl::init("main"), llvm::cl::cat(category));
llvm::cl::opt<std::string> output("output", llvm::cl::desc("Set an output file"), llvm::cl::Optional, llvm::cl::cat(category));
llvm::cl::list<std::string> includes(
    "I", 
    llvm::cl::Prefix,
    llvm::cl::desc("Add an include path"), 
    llvm::cl::value_desc("path"), 
    llvm::cl::cat(category)
);
llvm::cl::list<std::string> libraries(
    "l",
    llvm::cl::Prefix,
    llvm::cl::desc("Add a library"), 
    llvm::cl::value_desc("name"), 
    llvm::cl::cat(category)
);
llvm::cl::list<std::string> files(llvm::cl::Positional, llvm::cl::desc("<files>"), llvm::cl::OneOrMore);

struct Arguments {
    utils::filesystem::Path file;
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

    args.file = files.front(); 
    if (!args.file.exists()) {
        Compiler::error("File not found '{0}'", args.file.str()); exit(1);
    }

    args.entry = entry;   
    args.format = format;
    args.optimize = optimize;
    args.verbose = verbose;
    args.includes = includes;

    args.libraries.names = libraries;

    if (output.empty()) {
        args.output = args.file.with_extension("o").str();
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

    CompilerOptions options = {
        .input = args.file,
        .output = args.output,
        .entry = args.entry,
        .target = args.target,
        .libs = args.libraries,
        .includes = {},
        .format = args.format,
        .optimization = args.optimize ? OptimizationLevel::Release : OptimizationLevel::Debug,
        .opts = OptimizationOptions {
            .enable = args.optimize,
            .mangle_style = mangling
        },
        .verbose = args.verbose,
        .object_files = {},
        .extras = {}
    };

    Compiler compiler(options);    
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

    compiler.add_library("c");
    compiler.add_library("pthread");

    compiler.add_include_path("lib/");

    compiler.add_object_file("lib/panic.o");

    compiler.define_preprocessor_macro("__file__", args.file.filename());

    compiler.compile().unwrap();

    llvm::llvm_shutdown();
    return 0;
}
