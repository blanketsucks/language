#include <quart/compiler.h>
#include <quart/llvm.h>
#include <quart/cl.h>

using namespace quart;

int main(int argc, char** argv) {
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
        .libs = args.libraries,
        .imports = {},
        .format = args.format,
        .optimization = args.optimize ? OptimizationLevel::Release : OptimizationLevel::Debug,
        .opts = OptimizationOptions {
            .enable = args.optimize,
            .mangle_style = args.mangle_style
        },
        .verbose = args.verbose,
        .standalone = args.standalone,
        .object_files = {},
        .extras = {}
    };

    Compiler compiler(options);    
    for (auto& import : args.imports) {
        if (!fs::exists(import)) {
            Compiler::error("Could not find import path '{0}'", import); exit(1);
        }
        
        if (!fs::isdir(import)) {
            Compiler::error("Include path must be a directory"); exit(1);
        }

        compiler.add_import_path(import);
    }

    if (!args.standalone) {
        compiler.add_library("c");
    } else {
        compiler.set_linker("ld");
    }

    compiler.add_import_path(QUART_PATH);

    compiler.compile().unwrap();
    
    Compiler::shutdown();
    return 0;
}