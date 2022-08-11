#include "utils.h"

#include <assert.h>

void utils::error(Location location, const std::string& message, bool fatal) {
    std::cerr << fmt::format("{bold|white} {bold|red} {s}", location.format().c_str(), "error:", message);
    std::cerr << std::endl;
    
    if (fatal) {
        exit(1);
    }
}

void utils::note(Location location, const std::string& message) {
    std::cout << fmt::format("{bold|white} {bold|magenta} {s}", location.format().c_str(), "note:", message);
    std::cout << std::endl;
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