#include "compiler.h"
#include "include.h"
#include "utils.h"
#include <system_error>

#define _DEBUG_MODULE 0


struct Arguments {
    std::string filename;
    std::string output;
    std::string entry;

    std::vector<std::string> includes;
    Libraries libraries;

    OutputFormat format;
    bool optimize = true;
};

utils::argparse::ArgumentParser create_argument_parser() {
    utils::argparse::ArgumentParser parser = utils::argparse::ArgumentParser(
        "proton", 
        "Compiler for the Proton programming language.",
        "proton [options] ...files"
    );

    parser.add_argument(
        "--output", 
        utils::argparse::Required, 
        "-o",
        "Set an output file.", 
        "file"
    );

    parser.add_argument(
        "--entry", 
        utils::argparse::Required, 
        "-e", 
        "Set an entry point for the program.", 
        "name"
    );

    parser.add_argument(
        "-emit-llvm-ir", 
        utils::argparse::NoArguments, 
        EMPTY, 
        "Emit LLVM IR."
    );
    
    parser.add_argument(
        "-emit-llvm-bc", 
        utils::argparse::NoArguments, 
        EMPTY, 
        "Emit LLVM Bitcode."
    );
    
    parser.add_argument(
        "-emit-assembly", 
        utils::argparse::NoArguments, 
        "-S", 
        "Emit assembly code."
    );
    
    parser.add_argument(
        "-c", 
        utils::argparse::NoArguments, 
        EMPTY, 
        "Emit object code."
    );
    
    parser.add_argument(
        "-I", 
        utils::argparse::Many, 
        EMPTY, 
        "Add an include path.", 
        "path"
    );
    
    parser.add_argument(
        "-O0", 
        utils::argparse::NoArguments, 
        EMPTY, 
        "Disable optimizations."
    );

    parser.add_argument(
        "--library", 
        utils::argparse::Many, 
        "-l", 
        EMPTY, 
        "libname"
    );

    parser.add_argument(
        "-L",
        utils::argparse::Many,
        EMPTY,
        "Add directory to library search path.",
        "dir"
    );

    return parser;
}

std::vector<std::string> get_string_vector(utils::argparse::ArgumentParser parser, const std::string& name) {
    std::vector<std::string> result;

    auto args = parser.get<std::vector<llvm::Any>>(name, {});
    for (auto& arg : args) {
        result.push_back(llvm::any_cast<char*>(arg));
    }

    return result;

}

Arguments parse_arguments(int argc, char** argv) {
    Arguments args;
    utils::argparse::ArgumentParser parser = create_argument_parser();

    if (argc < 2) {
        parser.display_help();
        exit(0);
    }

    std::vector<std::string> filenames = parser.parse(argc, argv);
    if (filenames.empty()) {
        std::string fmt = utils::fmt::format("{bold|white} {bold|red} No input files specified.", "proton:", "error:");
        std::cerr << fmt << std::endl;

        exit(1);
    }

    std::string filename = filenames.back();

    args.entry = parser.get("entry", "main");
    if (parser.get("c", false)) {
        args.format = OutputFormat::Object;
    } else if (parser.get("emit-llvm-ir", false)) {
        args.format = OutputFormat::LLVM;
    } else if (parser.get("emit-assembly", false)) {
        args.format = OutputFormat::Assembly;
    } else if (parser.get("emit-llvm-bc", false)) {
        args.format = OutputFormat::Bitcode;
    } else {
        args.format = OutputFormat::Executable;
    }

    args.optimize = !parser.get("O0", false);
    args.filename = filename;

    if (!parser.has_value("output")) {
        args.output = utils::filesystem::replace_extension(filename, "o");
        switch (args.format) {
            case OutputFormat::LLVM:
                args.output = utils::filesystem::replace_extension(args.output, "ll"); break;
            case OutputFormat::Bitcode:
                args.output = utils::filesystem::replace_extension(args.output, "bc"); break;
            case OutputFormat::Assembly:
                args.output = utils::filesystem::replace_extension(args.output, "s"); break;
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
        args.output = parser.get<char*>("output");
    }

    args.libraries.names = get_string_vector(parser, "library");
    args.libraries.paths = get_string_vector(parser, "L");

    args.includes = get_string_vector(parser, "I");
    return args;
}

int main(int argc, char** argv) {
    Arguments args = parse_arguments(argc, argv);
    Compiler::init();

    utils::filesystem::Path path(args.filename);
    if (!path.exists()) {
        Compiler::error("File not found '{s}'", args.filename);
    }

    Compiler compiler;

    compiler.set_entry_point(args.entry);
    compiler.set_input_file(args.filename);
    compiler.set_output_file(args.output);
    compiler.set_output_format(args.format);

    compiler.set_optimization_level(args.optimize ? OptimizationLevel::Release : OptimizationLevel::Debug );
    
    for (auto& include : args.includes) {
        utils::filesystem::Path inc(include);
        if (!inc.exists()) {
            Compiler::error("Could not find include path '{s}'", include);
        }

        if (!inc.isdir()) {
            Compiler::error("Include path must be a directory");
        }

        compiler.add_include_path(include);
    }

    for (auto& library : args.libraries.names) {
        compiler.add_library(library);
    }

    for (auto& path : args.libraries.paths) {
        compiler.add_library_path(path);
    }

    compiler.add_library("c");

    compiler.add_include_path("lib/");
    compiler.define_preprocessor_macro("__file__", args.filename);

    CompilerError error = compiler.compile();
    if (error.code > 0) {
        if (!error.message.empty()) {
            Compiler::error(error.message);
        }

        return error.code;
    }

    return 0;
}