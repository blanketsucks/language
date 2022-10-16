#include "objects.h"

Struct::Struct(
    std::string name,
    bool opaque,
    llvm::StructType* type,
    std::map<std::string, StructField> fields
) : name(name), type(type), fields(fields), opaque(opaque) {
    this->methods = {};
    this->locals = {};
}

bool Struct::has_method(std::string name) { 
    return this->methods.find(name) != this->methods.end(); 
}

int Struct::get_field_index(std::string name) {
    if (this->fields.find(name) == this->fields.end()) {
        return -1;
    }

    return this->fields[name].index;
}

StructField Struct::get_field_at(uint32_t index) {
    std::vector<StructField> fields = utils::values(this->fields);
    std::sort(fields.begin(), fields.end(), [](auto& a, auto& b) { return a.index < b.index; });

    return fields[index];
}

std::vector<StructField> Struct::get_fields(bool with_private) {
    std::vector<StructField> fields;
    for (auto pair : this->fields) {
        if (pair.second.is_private) {
            if (with_private) {
                fields.push_back(pair.second);
            }   
        } else {
            fields.push_back(pair.second);
        }
    }

    return fields;
}

llvm::Value* Struct::get_variable(std::string name) {
    auto iter = this->locals.find(name);
    if (iter == this->locals.end()) {
        return nullptr;
    }

    return iter->second;
}

std::vector<Struct*> Struct::expand() {
   std::vector<Struct*> parents;

    parents.push_back(this);
    for (Struct* parent : this->parents) {
        parents.push_back(parent);

        std::vector<Struct*> expanded = parent->expand();
        parents.insert(parents.end(), expanded.begin(), expanded.end());
    }

    return parents;
}