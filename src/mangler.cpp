#include <quart/mangler.h>
#include <quart/utils/string.h>

#include <sstream>

const char* Mangler::PREFIX = "P";
const char* Mangler::SEPARATOR = "3";
const char* Mangler::ARGS_SEPARATOR = ".";
const char* Mangler::VARIADIC = "e";
const char* Mangler::END = "E";

std::string Mangler::replace(std::string str) {
    for (auto& c : str) {
        if (c == '.') { c = *Mangler::SEPARATOR; }
    }

    return str;
}

std::string Mangler::mangle(llvm::Type* type) {
    std::stringstream stream;

    if (type->isVoidTy()) {
        stream << "v";
    } else if (type->isIntegerTy()) {
        stream << "i" << type->getIntegerBitWidth();
    } else if (type->isFloatingPointTy()) {
        stream << "f" << type->getPrimitiveSizeInBits();
    } else if (type->isPointerTy()) {
        stream << "p" << Mangler::mangle(type->getPointerElementType());
    } else if (type->isArrayTy()) {
        stream << "a" << type->getArrayNumElements() << Mangler::mangle(type->getArrayElementType());
    } else if (type->isStructTy()) {
        stream << "S" << Mangler::replace(type->getStructName().str());
    } else if (type->isFunctionTy()) {
        auto ftype = llvm::cast<llvm::FunctionType>(type);
        stream << "F" << Mangler::mangle(ftype->getReturnType());
        for (auto& arg : ftype->params()) {
            stream << Mangler::mangle(arg);
        }

        if (ftype->isVarArg()) {
            stream << Mangler::VARIADIC;
        }
    } else {
        stream << "u" << type->getPrimitiveSizeInBits();
    }

    return stream.str();
}

std::string Mangler::mangle(
    const std::string& name, 
    std::vector<llvm::Type*> args,
    bool is_variadic,
    llvm::Type* ret,
    utils::Ref<Namespace> ns,
    utils::Ref<Struct> structure,
    utils::Ref<Module> module
) {
    std::stringstream stream; stream << '_' << Mangler::PREFIX;

    if (module) stream << Mangler::replace(module->name) << Mangler::SEPARATOR; 
    if (ns) stream << Mangler::replace(ns->qualified_name) << Mangler::SEPARATOR; 
    if (structure) stream << Mangler::replace(structure->qualified_name) << Mangler::SEPARATOR; 

    stream << name;
    if (!args.empty()) {
        stream << Mangler::ARGS_SEPARATOR;
        for (auto& arg : args) {
            stream << Mangler::mangle(arg) << Mangler::ARGS_SEPARATOR;
        }

        stream.seekp(-1, std::ios_base::end);
        if (is_variadic) {
            stream << Mangler::VARIADIC;
        }
    }

    if (ret) {
        stream << Mangler::ARGS_SEPARATOR << Mangler::mangle(ret);
    }

    return stream.str();
}

std::string Mangler::demangle_type(std::string name) {
    std::stringstream stream;
    switch (name[0]) {
        case 'v': stream << "void"; break;
        case 'i': {
            int size = std::stoi(name.substr(1));
            switch (size) {
                case 1: stream << "bool"; break;
                case 8: stream << "char"; break;
                case 16: stream << "short"; break;
                case 32: stream << "int"; break;
                case 64: stream << "long"; break;
                default: stream << "i" << size; break;
            }

            break;
        }
        case 'f': {
            int size = std::stoi(name.substr(1));
            switch (size) {
                case 32: stream << "float"; break;
                case 64: stream << "double"; break;
                default: stream << "f" << size; break;
            }

            break;
        }
        case 'p': stream << Mangler::demangle_type(name.substr(1)) << "*"; break;
        case 'a': {
            uint32_t i = 1;
            std::string size;

            while (std::isdigit(name[i])) { 
                size += name[i];
                i++; 
            }

            stream << "[" << Mangler::demangle_type(name.substr(i)) << "; " << size << "]"; break;
        }
        case 'S': stream << utils::replace(name.substr(1), "3", "::"); break;
        case 'F': stream << "func"; break; // TODO: implement
    }

    return stream.str();
}

std::string Mangler::demangle(std::string name) {
    name = name.substr(2);
    std::stringstream stream;

    auto parts = utils::split(name, Mangler::ARGS_SEPARATOR);
    name = parts[0];

    stream << "func " << utils::replace(name, "3", "::");
    std::string ret = Mangler::demangle_type(parts.back());

    stream << "(";
    if (parts.size() > 2) {
        for (uint32_t i = 1; i < parts.size() - 1; i++) {
            stream << Mangler::demangle_type(parts[i]) << ", ";
        }

        stream.seekp(-2, std::ios_base::end);
    }

    stream << ") -> " << ret;
    return stream.str();
}