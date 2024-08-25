#include <quart/compiler.h>
#include <quart/cl.h>

using namespace quart;

int main(int argc, char** argv) {
    ErrorOr<cl::Arguments> result = cl::parse_arguments(argc, argv);
    if (result.is_err()) {
        auto error = result.error();
        errln("\x1b[1;37mquart: \x1b[1;31merror: \x1b[0m{0}", error.message());

        return 1;
    }

    cl::Arguments args = result.release_value();
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
        .no_libc = args.no_libc,
        .object_files = {},
        .extras = {}
    };

    Compiler compiler(move(options));
    return compiler.compile();
}