#include <quart/compiler.h>
#include <quart/llvm.h>
#include <quart/cl.h>

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
        .includes = {},
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
    } else {
        compiler.set_linker("ld");
    }

    compiler.add_include_path("lib/");

    compiler.compile().unwrap();
    
    Compiler::shutdown();
    return 0;
}