#include <llvm-14/llvm/Support/CommandLine.h>
#include <quart/cl.h>

#include <llvm/Support/CommandLine.h>

using namespace quart;

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

llvm::cl::opt<MangleStyle> mangle_style(
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
llvm::cl::list<std::string> imports(
    "I", 
    llvm::cl::Prefix,
    llvm::cl::desc("Add an import path"), 
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
llvm::cl::opt<bool> jit("jit", llvm::cl::desc("Run the program in the JIT"), llvm::cl::init(false), llvm::cl::cat(category));
llvm::cl::list<std::string> files(llvm::cl::Positional, llvm::cl::desc("<files>"), llvm::cl::ZeroOrMore);

cl::Arguments cl::parse_arguments(int argc, char** argv) {
    cl::Arguments args;

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
        Compiler::error("File not found '{0}'", args.file.name); exit(1);
    }

    args.entry = entry;   
    args.format = format;
    args.optimize = optimize;
    args.verbose = verbose;
    args.imports = imports;
    args.standalone = standalone;
    args.target = target;
    args.mangle_style = mangle_style;
    args.jit = jit;

    args.library_names = std::set<std::string>(libraries.begin(), libraries.end());

    if (output.empty()) {
        llvm::StringRef extension = OUTPUT_FORMATS_TO_EXT.at(args.format);
        args.output = args.file.with_extension(extension.str());
    } else {
        args.output = output;
    }

    return args;
}