#include <quart/compiler.h>
#include <quart/llvm.h>

#include "llvm/Support/CommandLine.h"

llvm::cl::OptionCategory category("Compiler options");

llvm::cl::opt<bool> verbose(
    "verbose", llvm::cl::desc("Enable verbose output"), llvm::cl::init(false), llvm::cl::cat(category)
);
llvm::cl::opt<bool> optimize(
    "optimize", llvm::cl::desc("Enable optimizations"), llvm::cl::init(false), llvm::cl::cat(category)
);
llvm::cl::opt<bool> standalone(
    "standalone", llvm::cl::desc("Link with libc"), llvm::cl::init(false), llvm::cl::cat(category)
);
llvm::cl::opt<bool> print_all_targets(
    "print-all-targets", llvm::cl::desc("Print all available targets"), llvm::cl::init(false), llvm::cl::cat(category)
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
    llvm::cl::init(MangleStyle::Minimal), 
    llvm::cl::values(
        clEnumValN(MangleStyle::Full, "full", "Fully mangle names"),
        clEnumValN(MangleStyle::Minimal, "minimal", "Use a minimal mangling style (default)"),
        clEnumValN(MangleStyle::None, "none", "Do not mangle names")
    ),
    llvm::cl::cat(category)
);

llvm::cl::opt<std::string> entry("entry", llvm::cl::desc("Set an entry point for the program"), llvm::cl::init("main"), llvm::cl::cat(category));
llvm::cl::opt<std::string> output("output", llvm::cl::desc("Set an output file"), llvm::cl::Optional, llvm::cl::cat(category));
llvm::cl::opt<std::string> target("target", llvm::cl::desc("Set the target triple for which the code is compiled"), llvm::cl::Optional, llvm::cl::cat(category));
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
llvm::cl::list<std::string> files(llvm::cl::Positional, llvm::cl::desc("<files>"), llvm::cl::ZeroOrMore);

struct Arguments {
    utils::fs::Path file;
    std::string output;
    std::string entry;
    std::string target;

    std::vector<std::string> includes;
    Libraries libraries;

    OutputFormat format;
    bool optimize;
    bool verbose;
    bool standalone;
    bool print_all_targets;
    bool jit;
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;

    llvm::cl::HideUnrelatedOptions(category);
    llvm::cl::ParseCommandLineOptions(argc, argv);

    if (files.empty() && !print_all_targets) {
        Compiler::error("No input files"); exit(1);
    }

    if (print_all_targets) {
        args.print_all_targets = true;
        return args;
    }

    args.file = files.front(); 
    if (!args.file.exists()) {
        Compiler::error("File not found '{0}'", args.file.str()); exit(1);
    }

    args.entry = entry;   
    args.format = format;
    args.optimize = optimize;
    args.verbose = verbose;
    args.includes = includes;
    args.standalone = standalone;
    args.target = target;

    args.libraries.names = std::set<std::string>(libraries.begin(), libraries.end());

    if (output.empty()) {
        args.output = args.file.with_extension("o").str();
        switch (args.format) {
            case OutputFormat::LLVM:
                args.output = utils::fs::replace_extension(args.output, "ll"); break;
            case OutputFormat::Bitcode:
                args.output = utils::fs::replace_extension(args.output, "bc"); break;
            case OutputFormat::Assembly:
                args.output = utils::fs::replace_extension(args.output, "s"); break;
            case OutputFormat::SharedLibrary:
            #if _WIN32 || _WIN64
                args.output = utils::fs::replace_extension(args.output, "lib"); break;
            #else
                args.output = utils::fs::replace_extension(args.output, "so"); break;
            #endif
            case OutputFormat::Executable:
            #if _WIN32 || _WIN64
                args.output = utils::fs::replace_extension(args.output, "exe"); break;
            #else
                args.output = utils::fs::remove_extension(args.output); break;
            #endif
            default:
                args.output = utils::fs::replace_extension(args.output, "o"); break;
        }
    } else {
        args.output = output;
    }

    return args;
}

int main(int argc, char** argv) {
    Arguments args = parse_arguments(argc, argv);
    Compiler::init();

    if (args.print_all_targets) {
        llvm::TargetRegistry::printRegisteredTargetsForVersion(llvm::outs());
        return 0;
    }

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
        .standalone = args.standalone,
        .object_files = {},
        .extras = {}
    };

    Compiler compiler(options);    
    for (auto& include : args.includes) {
        utils::fs::Path inc(include);
        if (!inc.exists()) {
            Compiler::error("Could not find include path '{0}'", include); exit(1);
        }
        
        if (!inc.isdir()) {
            Compiler::error("Include path must be a directory"); exit(1);
        }

        compiler.add_include_path(include);
    }

    if (!args.standalone) {
        compiler.add_library("c");
        compiler.add_library("pthread");

        compiler.add_object_file("lib/panic.o");
    } else {
        compiler.set_linker("ld");
    }

    compiler.add_include_path("lib/");

    compiler.compile().unwrap();
    
    Compiler::shutdown();
    return 0;
}