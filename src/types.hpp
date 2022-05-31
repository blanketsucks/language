#ifndef _TYPES_H
#define _TYPES_H

#include <string>
#include <vector>

// class Type {
// public:
//     enum Value {
//         Short,
//         Integer,
//         Int64,
//         Float,
//         String,
//     };

//     Type(Value value);

//     bool is_compatible(Type other);

//     Value value;
// };

struct Type {
    std::string name;
    int size;
    bool is_number = true;
    bool signed_ = true;
};

static std::vector<Type> TYPES = {
    {"short", 2},
    {"int", 4},
    {"long", 8},
    {"float", 4},
    {"double", 8},
    {"str", 1, false, false},
    {"void", 0, false, false}
};

#endif