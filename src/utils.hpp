#ifndef _UTILS_HPP
#define _UTILS_HPP

#include <iostream>

#define TODO(x) std::cout << "TODO: " << "(" << __FILE__ << ":" << __LINE__ << ") " << x << '\n'; exit(1)
#define UNUSED(x) (void)x

std::string get_source(Location* start, Location* end) { 
    return start->source.substr(start->index - 1, end->index - start->index + 1);
}

#endif