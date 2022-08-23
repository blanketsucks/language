#include "include.h"

#if __clang__
    #define SOURCE_COMPILER "clang"
#elif __GNUC__
    #define SOURCE_COMPILER "gcc"
#else
    #define SOURCE_COMPILER "mscv"
#endif

enum class OutputFormat {
    Object,
    LLVM,
    Assembly,
    Executable
};

struct Arguments {
    std::string filename;
    std::string output;
    std::string entry;

    std::vector<std::string> includes;
    std::vector<std::string> libraries;

    OutputFormat format;
    bool libc = true;
    bool lex = false;
    bool optimize = true;
};

utils::argparse::ArgumentParser create_argument_parser() {
    utils::argparse::ArgumentParser parser = utils::argparse::ArgumentParser(
        "proton", 
        "Compiler for the Proton programming language.",
        "proton [options] ...files"
    );

    parser.add_argument("--output", utils::argparse::Required, "-o", "The output file.", "file");
    parser.add_argument("--entry", utils::argparse::Required, "-e", "The main entry point for the program.", "function");
    parser.add_argument("-emit-llvm", utils::argparse::NoArguments, EMPTY, "Emit LLVM IR.");
    parser.add_argument("-emit-assembly", utils::argparse::NoArguments, "-S", "Emit assembly code.");
    parser.add_argument("-c", utils::argparse::NoArguments, EMPTY, "Emit object code.");
    parser.add_argument("-nolibc", utils::argparse::NoArguments, EMPTY, "Disable the use of libc.");
    parser.add_argument("-lex", utils::argparse::NoArguments, EMPTY, "Print the lexer output. Note the tokens are after the preprocessor.");
    parser.add_argument("-I", utils::argparse::Many, EMPTY, "Add an include path/folder.", "paths");
    parser.add_argument("-O0", utils::argparse::NoArguments, EMPTY, "Disable optimizations.");
    parser.add_argument("-l", utils::argparse::Many, EMPTY);

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
    } else if (parser.get("emit-llvm", false)) {
        args.format = OutputFormat::LLVM;
    } else if (parser.get("emit-assembly", false)) {
        args.format = OutputFormat::Assembly;
    } else {
        args.format = OutputFormat::Executable;
    }

    args.libc = !parser.get("nolibc", false);
    args.lex = parser.get("lex", false);
    args.optimize = !parser.get("O0", false);
    args.filename = filename;

    if (!parser.has_value("output")) {
        args.output = utils::filesystem::replace_extension(filename, "o");
        switch (args.format) {
            case OutputFormat::LLVM:
                args.output = utils::filesystem::replace_extension(args.output, "ll"); break;
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

    args.libraries = get_string_vector(parser, "l");

    return args;
}

void init() {
    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();
}

int main(int argc, char** argv) {
    Arguments args = parse_arguments(argc, argv);
    init();

    std::fstream file(args.filename, std::ios::in);
    utils::filesystem::Path path(args.filename);

    if (!path.exists()) {
        std::string fmt = utils::fmt::format(
            "{bold|white} {bold|red} File not found: '{s}'",
            "proton:", "error:", args.filename
        );

        std::cerr << fmt << std::endl;
        return 1;
    }

    Lexer lexer(file, args.filename);
    Preprocessor preprocessor(lexer.lex(), {"lib/"});

#if _WIN64
    preprocessor.define("__WIN64__", 1);
#elif _WIN32
    preprocessor.define("__WIN32__", 1);
#elif __linux__
    preprocessor.define("__LINUX__", 1);
#endif

    if (args.libc) {
        preprocessor.define("__LIBC__", 1);
    }

    std::vector<Token> tokens = preprocessor.process();
    if (args.lex) {
        for (auto token : tokens) {
            std::string repr = utils::fmt::format("Token(type={i}, value='{s}')", (int)token.type, token.value);
            std::string location = utils::fmt::format("{bold|white}", token.start.format().c_str());

            std::cout << location << ": " << repr << std::endl;
        }

        return 0;
    }
    
    Parser parser(tokens);
    auto ast = parser.statements();

    Visitor visitor(args.filename, args.optimize);

    visitor.visit(std::move(ast));

    visitor.cleanup();
    visitor.free();

    if (args.format == OutputFormat::Executable) {
        llvm::Function* entry = visitor.module->getFunction(args.entry);
        if (!entry) {
            std::string fmt = utils::fmt::format(
                "{bold|white} {bold|red} Entry point '{s}' not found.",
                "proton:", "error:", args.entry
            );

            std::cerr << fmt << std::endl;
            return 1;
        }
    }

    std::string target_triple = llvm::sys::getDefaultTargetTriple();
    std::string err;

    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(target_triple, err);
    if (!target) {
        std::cerr << utils::fmt::format("{bold|red}: Could not create target '{s}'. {s}", "error", target_triple, err) << std::endl;
        return 1;
    }

    llvm::TargetOptions options;
    auto reloc = llvm::Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    llvm::TargetMachine* target_machine = target->createTargetMachine(target_triple, "generic", "", options, reloc, llvm::None, llvm::CodeGenOpt::Aggressive);
    
    visitor.module->setDataLayout(target_machine->createDataLayout());
    visitor.module->setTargetTriple(target_triple);

    visitor.module->setPICLevel(llvm::PICLevel::BigPIC);
    visitor.module->setPIELevel(llvm::PIELevel::Large);

    std::error_code error;

    std::string object = args.output;
    if (!utils::filesystem::has_extension(object)) {
        object = object + ".o";
    }

    llvm::raw_fd_ostream dest(object, error, llvm::sys::fs::OF_None);
    if (error) {
        std::cerr << utils::fmt::format("{bold|red}: Could not open file '{s}'. {s}", "error", object, error.message()) << std::endl;
        return 1;
    }

    if (args.format == OutputFormat::LLVM) {
        visitor.module->print(dest, nullptr);

        delete target_machine;
        return 0;
    }

    llvm::legacy::PassManager pass;

    llvm::CodeGenFileType type = llvm::CodeGenFileType::CGFT_ObjectFile;
    if (args.format == OutputFormat::Assembly) {
        type = llvm::CodeGenFileType::CGFT_AssemblyFile;
    }

    if (target_machine->addPassesToEmitFile(pass, dest, nullptr, type)) {
        std::cerr << utils::fmt::format("{bold|red}: Target machine can't emit a file of this type", "error") << std::endl;
        return 1;
    }

#if _DEBUG_MODULE
    visitor.dump(llvm::outs());
#endif

    pass.run(*visitor.module);
    dest.flush();

    delete target_machine;
    if (args.format != OutputFormat::Executable) {
        return 0;
    }

    std::string libraries = utils::fmt::join(" ", args.libraries);
    std::string compiler = args.libc ? SOURCE_COMPILER : "ld";

    std::vector<std::string> arguments = {
        "-o", args.output,
        object
    };

    if (!libraries.empty()) {
        arguments.push_back("-l"); arguments.push_back(libraries);
    }

    if (!args.libc || (args.entry != "main")) {
        arguments.push_back("-e"); arguments.push_back(args.entry);
    }

    std::string command = utils::fmt::format("{s} {s}", compiler, utils::fmt::join(" ", arguments));
    return std::system(command.c_str());
}