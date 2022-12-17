#include "utils/log.h"

bool utils::has_color_support() {
    return isatty(fileno(stdout));
}

std::string utils::color(Color color, const std::string& str) {
    if (utils::has_color_support()) {
        return FORMAT("\033[1;{0}m", int(color)) + str + "\033[0;0m";
    } else {
        return str;
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

