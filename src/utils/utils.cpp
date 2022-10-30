#include "utils.h"

#include <assert.h>

std::string utils::join(std::string sep, std::vector<std::string> strings) {
    std::string result;
    for (size_t i = 0; i < strings.size(); i++) {
        result += strings[i];
        if (i < strings.size() - 1) {
            result += sep;
        }
    }

    return result;
}

std::vector<std::string> utils::split(std::string str, char delimiter) {
    std::vector<std::string> result;
    size_t pos = 0;

    std::string token;
    while ((pos = str.find(delimiter)) != std::string::npos) {
        token = str.substr(0, pos);
        result.push_back(token);

        str.erase(0, pos + 1);
    }

    result.push_back(str);
    return result;
}

std::string utils::replace(std::string str, std::string from, std::string to) {
    size_t start = 0;
    while ((start = str.find(from, start)) != std::string::npos) {
        str.replace(start, from.length(), to);
        start += to.length();
    }

    return str;
}


bool utils::has_color_support() {
    return isatty(fileno(stdout));
}

std::string utils::color(const std::string& color, const std::string& s) {
    if (utils::has_color_support()) {
        return color + s + RESET;
    } else {
        return s;
    }
}

void utils::error(Location location, const std::string& message, bool fatal) {
    std::cout << utils::color(WHITE, location.format()) << ' ';
    std::cout << utils::color(RED, "error:") << ' ';

    std::cout << message << std::endl;
    if (fatal) {

        exit(1);
    }
}

void utils::note(Location location, const std::string& message) {
    std::cout << utils::color(WHITE, location.format()) << ' ';
    std::cout << utils::color(MAGENTA, "note:") << ' ';

    std::cout << message << std::endl;
}

std::string utils::exec(const std::string& command) {
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen(command.c_str(), "r");
    assert(pipe && "Failed to invoke popen().");

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    pclose(pipe);
    return result;
}
