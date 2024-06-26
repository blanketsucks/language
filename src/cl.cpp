#include <quart/cl.h>
#include <quart/errors.h>

#include <llvm/Support/CommandLine.h>

namespace quart::cl {

llvm::cl::OptionCategory category("Compiler options"); // NOLINT

const llvm::cl::opt<bool> verbose(
    "verbose",
    llvm::cl::desc("Enable verbose output"), 
    llvm::cl::init(false), 
    llvm::cl::cat(category)
);

const llvm::cl::opt<bool> optimize(
    "optimize", 
    llvm::cl::desc("Enable optimizations"), 
    llvm::cl::init(false), 
    llvm::cl::cat(category)
);

const llvm::cl::opt<bool> standalone(
    "standalone",
    llvm::cl::desc("When provided libc doesn't get linked with the final executable"),
    llvm::cl::init(false),
    llvm::cl::cat(category)
);

const llvm::cl::opt<bool> print_all_targets(
    "print-all-targets",
    llvm::cl::desc("Print all available targets"),
    llvm::cl::init(false),
    llvm::cl::cat(category)
);

const llvm::cl::opt<OutputFormat> format(
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

const llvm::cl::opt<MangleStyle> mangle_style(
    "mangle-style", 
    llvm::cl::desc("Mangling style for symbols"), 
    llvm::cl::init(MangleStyle::Minimal), 
    llvm::cl::values(
        clEnumValN(MangleStyle::Full, "full", "Fully mangle names"),
        clEnumValN(MangleStyle::Minimal, "minimal", "Use a minimal mangling style (default)"),
        clEnumValN(MangleStyle::None, "none", "Do not mangle names")
    ),
    llvm::cl::cat(category)
);

const llvm::cl::opt<String> entry(
    "entry",
    llvm::cl::desc("The entry point of the executable."),
    llvm::cl::init("main"),
    llvm::cl::cat(category)
);


const llvm::cl::opt<String> output(
    "output", "o",
    llvm::cl::desc("Set an output file"),
    llvm::cl::Optional,
    llvm::cl::cat(category)
);

const llvm::cl::opt<String> target(
    "target",
    llvm::cl::desc("Set the target triple for which the code is compiled"),
    llvm::cl::Optional,
    llvm::cl::cat(category)
);

const llvm::cl::list<String> imports(
    "I", 
    llvm::cl::Prefix,
    llvm::cl::desc("Add an import path"), 
    llvm::cl::value_desc("path"), 
    llvm::cl::cat(category)
);

const llvm::cl::list<String> libraries(
    "l",
    llvm::cl::Prefix,
    llvm::cl::desc("Add a library"), 
    llvm::cl::value_desc("name"), 
    llvm::cl::cat(category)
);

const llvm::cl::opt<bool> jit(
    "jit",
    llvm::cl::desc("Run the program in the JIT"), 
    llvm::cl::init(false),
    llvm::cl::cat(category)
);

const llvm::cl::list<String> files(llvm::cl::Positional, llvm::cl::desc("<files>"), llvm::cl::ZeroOrMore);

ErrorOr<Arguments> parse_arguments(int argc, char** argv) {
    Arguments args;

    llvm::cl::HideUnrelatedOptions(category);
    llvm::cl::ParseCommandLineOptions(argc, argv);

    if (files.empty() && !print_all_targets) {
        return err(Span {}, "No input files provided");
    }

    if (print_all_targets) {
        args.print_all_targets = true;
        return args;
    }

    args.file = files.front(); 
    if (!args.file.exists()) {
        return err(Span {}, "File '{0}' does not exist", args.file);
    } else if (!args.file.is_regular_file()) {
        return err(Span {}, "'{0}' is not a regular file", args.file);
    }

    args.entry = move(entry);   
    args.format = format;
    args.optimize = optimize;
    args.verbose = verbose;
    args.imports = Vector<String>(imports.begin(), imports.end());
    args.standalone = standalone;
    args.target = target;
    args.mangle_style = mangle_style;
    args.jit = jit;

    args.library_names = std::set<String>(libraries.begin(), libraries.end());

    if (output.empty()) {
        llvm::StringRef extension = OUTPUT_FORMATS_TO_EXT.at(args.format);
        args.output = args.file.with_extension(extension.str());
    } else {
        args.output = output;
    }

    return args;
}

}