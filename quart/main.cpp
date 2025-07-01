#include <quart/compiler.h>
#include <quart/cl.h>

#include <llvm/Support/ManagedStatic.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Support/TargetSelect.h>

using namespace quart;

int main(int argc, char** argv) {
    llvm::llvm_shutdown_obj shutdown;

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();

    ErrorOr<cl::Arguments> result = cl::parse_arguments(argc, argv);
    if (result.is_err()) {
        auto error = result.error();
        errln("\x1b[1;37mquart: \x1b[1;31merror: \x1b[0m{}", error.message());

        return 1;
    }

    cl::Arguments args = result.release_value();
    if (args.print_all_targets) {
        llvm::TargetRegistry::printRegisteredTargetsForVersion(llvm::outs());
        return 0;
    }

    CompilerOptions options = {
        .file = args.file,
        .output = args.output,
        .entry = args.entry,
        .target = args.target,
        .library_names = args.library_names,
        .library_paths = args.library_paths,
        .imports = {},
        .format = args.format,
        .opts = OptimizationOptions {
            .level = args.optimization_level,
            .mangle_style = args.mangle_style
        },
        .verbose = args.verbose,
        .no_libc = args.no_libc,
        .object_files = {},
        .extras = {}
    };

    Compiler compiler(move(options));
    if (args.verbose) {
        compiler.dump();
    }

    return compiler.compile();
}