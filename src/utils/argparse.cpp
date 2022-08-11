#include "utils.h"

using namespace utils::argparse;

Argument::Argument() {
    this->name = EMPTY;
    this->short_name = EMPTY;
    this->description = EMPTY;
    this->dest = "arg";
    this->type = ArgumentValueType::NoArguments;
    this->required = false;
}

Argument::Argument(
    std::string name, 
    ArgumentValueType type, 
    std::string short_name, 
    std::string description, 
    std::string dest,
    bool required,
    CallbackFunction callback
) {
    this->name = name;
    this->short_name = short_name;
    this->description = description;
    this->type = type;
    this->dest = dest;
    this->required = required;
    this->callback = callback;
}

std::string Argument::get_clean_name() {
    if (this->name.size() > 2 && this->name.substr(0, 2) == "--") {
        return this->name.substr(2);
    } else if (this->name.size() > 1 && this->name.substr(0, 1) == "-") {
        return this->name.substr(1);
    } else {
        return this->name;
    }
}


ArgumentParser::ArgumentParser(
    std::string name, 
    std::string description, 
    std::string usage,
    std::string epilogue, 
    bool exit_on_error, 
    bool add_help
) {
    this->name = name;
    this->description = description;
    this->usage = usage;
    this->epilogue = epilogue;
    this->exit_on_error = exit_on_error;

    if (add_help) {
        this->add_argument("--help", ArgumentValueType::NoArguments, "-h", "Prints this message", "help", false, [this](llvm::Any) {
            this->display_help();
            exit(0);
        });
    }
}

void ArgumentParser::display_help() {
    if (this->usage.empty()) {
        std::cout << "Usage: " << this->name << " [options]" << "\n\n";
    } else {
        std::cout << "Usage: " << this->usage << "\n\n";
    }

    if (!this->description.empty()) {
        std::cout << this->description << "\n\n";
    }

    std::cout << "Options:" << '\n';
    std::map<std::string, Argument> seen;

    uint32_t alignment = 23;
    for (auto& pair : arguments) {
        Argument arg = pair.second;
        if (seen.find(arg.name) != seen.end()) {
            continue;
        }

        std::string description = "No description.";
        if (!arg.description.empty()) {
            description = arg.description;
        }

        std::string name = arg.name;
        if (!arg.short_name.empty()) {
            name = arg.short_name + ", " + name;
        }

        if (arg.type == ArgumentValueType::Optional) {
            name += " [" + arg.dest + "]";
        } else if (arg.type == ArgumentValueType::Required) {
            name += " <" + arg.dest + ">";
        } else if (arg.type == ArgumentValueType::Many) {
            name += " [" + arg.dest + "...]";
        }

        if (name.size() > alignment) {
            std::cout << "  " << name << '\n' << std::string(alignment + 2, ' ') << description << '\n';
        } else {
            int padding = alignment - name.size();
            std::cout << "  " << name << std::string(padding, ' ') << description << '\n';
        }

        seen[arg.name] = arg;
    }

    if (!this->epilogue.empty()) {
        std::cout << '\n' << this->epilogue << std::endl;
    }
}

void ArgumentParser::error(const std::string& message, ...) {
    va_list args;
    va_start(args, message);

    std::string msg = utils::fmt::format(message, args);
    va_end(args);

    std::cerr << utils::fmt::format("{bold|white}: {bold|red}: {s}\n", this->name.c_str(), "error", msg);
    if (this->exit_on_error) {
        exit(1);
    }
}

Argument ArgumentParser::add_argument(
    std::string name, 
    ArgumentValueType type, 
    std::string short_name, 
    std::string description, 
    std::string dest,
    bool required,
    CallbackFunction callback
) {
    Argument arg(name, type, short_name, description, dest, required, callback);
    return this->add_argument(arg);
}

Argument ArgumentParser::add_argument(Argument arg) {
    if (this->arguments.find(arg.name) != this->arguments.end()) {
        this->error("Argument '{}' already exists.", arg.name);
    }

    this->arguments[arg.name] = arg;
    if (!arg.short_name.empty()) {
        this->arguments[arg.short_name] = arg;
    }
    
    return arg;
}
std::vector<std::string> ArgumentParser::parse(int argc, char** argv) {
    int i = 1;
    std::vector<std::string> rest; 
    
    while (i < argc) {
        std::string name = argv[i];
        if (this->arguments.find(name) == this->arguments.end()) {
            i++;

            if (name[0] == '-') {
                this->error("Unrecognized command line argument -- '{s}'", name);
                continue;
            }

            rest.push_back(name);
            continue;
        }

        Argument argument = this->arguments[name];
        i++;

        if (this->has_value(argument)) {
            this->error("Argument is '{s}' already specified.", argument.name);

            continue;
        }

        if (argument.type == ArgumentValueType::NoArguments) {
            this->set_value(argument, true);
        } else if (argument.type == ArgumentValueType::Optional) {
            if (i >= argc) {
                continue;
            } 

            this->set_value(argument, argv[i]);
        } else if (argument.type == ArgumentValueType::Many) {
            std::vector<llvm::Any> values;
            if (this->has_value(argument)) {
                values = this->get<std::vector<llvm::Any>>(argument.get_clean_name());
            }

            while (i < argc) {
                if (argv[i][0] == '-') {
                    break;
                }

                values.push_back(argv[i]);
                i++;
            }

            this->set_value(argument, values);
        } else {
            if (i >= argc) {
                this->error("Argument '{s}' requires a value.", argument.name);
                continue;
            }

            this->set_value(argument, argv[i]);
        }
    }

    return rest;
}

bool ArgumentParser::has_value(std::string name) { 
    return this->values.find(name) != this->values.end(); 
}

bool ArgumentParser::has_value(Argument arg) { 
    return this->has_value(arg.get_clean_name()); 
}

void ArgumentParser::set_value(Argument arg, llvm::Any value) {
    this->values[arg.get_clean_name()] = value;
    if (arg.callback) {
        arg.callback(value);
    }
}
