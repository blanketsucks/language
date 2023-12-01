#include <quart/compiler.h>
#include <quart/llvm.h>
#include <quart/cl.h>

using namespace quart;

int run_jit(llvm::ArrayRef<char*> args);

int main(int argc, char** argv) {
    if (getenv("QUART_USE_JIT") != nullptr) {
        llvm::ArrayRef<char*> args(argv, argc);
        return run_jit(args);
    }

    cl::Arguments args = cl::parse_arguments(argc, argv);
    Compiler::init();

    if (args.print_all_targets) {
        llvm::TargetRegistry::printRegisteredTargetsForVersion(llvm::outs());
        Compiler::shutdown();

        return 0;
    }

    CompilerOptions options = {
        .input = args.file,
        .output = args.output,
        .entry = args.entry,
        .target = args.target,
        .library_names = args.library_names,
        .library_paths = args.library_paths,
        .imports = {},
        .format = args.format,
        .opts = OptimizationOptions {
            .level = args.optimize ? OptimizationLevel::Release : OptimizationLevel::Debug,
            .enable = args.optimize,
            .mangle_style = args.mangle_style
        },
        .verbose = args.verbose,
        .standalone = args.standalone,
        .object_files = {},
        .extras = {}
    };

    Compiler compiler(options);
    compiler.add_import_path(QUART_PATH);

    for (auto& import : args.imports) {
        if (!fs::exists(import)) {
            Compiler::error("Could not find import path '{0}'", import);
            return 1;
        }
        
        if (!fs::isdir(import)) {
            Compiler::error("Import path '{0}' must be a directory", import);
            return 1;
        }

        compiler.add_import_path(import);
    }

    if (!args.standalone) {
        compiler.add_library("c");
    } else {
        compiler.set_linker("ld");
    }

    compiler.compile().unwrap();    
    Compiler::shutdown();

    return 0;
}

int run_jit(llvm::ArrayRef<char*> args) {
    if (args.size() < 2) {
        Compiler::error("No input file specified");
        return 1;
    }

    fs::Path input(args[1]);
    if (!input.exists()) {
        Compiler::error("Input file '{0}' does not exist", input);
        return 1;
    }

    if (!input.isfile()) {
        Compiler::error("Input '{0}' must be a file", input);
        return 1;
    }

    Compiler::init();
    CompilerOptions options;

    options.input = input;
    options.entry = "main";

    Compiler compiler(options);
    compiler.add_import_path(QUART_PATH);

    return compiler.jit(args.slice(1));
}