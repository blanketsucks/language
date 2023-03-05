#include "objects/structs.h"
#include "objects/scopes.h"

Struct::Struct(
    std::string name,
    std::string qualified_name,
    bool opaque,
    llvm::StructType* type,
    std::map<std::string, StructField> fields
) : name(name), qualified_name(qualified_name), type(type), fields(fields), opaque(opaque) {
    this->impl = nullptr;
    this->scope = nullptr;
}

llvm::Type* Struct::get_self_type() {
    if (!this->impl) {
        return this->type;
    }

    return this->impl;
}

bool Struct::has_method(std::string name) { 
    return this->scope->functions.find(name) != this->scope->functions.end(); 
}

utils::Ref<Function> Struct::get_method(std::string name) { 
    return this->scope->functions[name]; 
}

int Struct::get_field_index(std::string name) {
    if (this->fields.find(name) == this->fields.end()) {
        return -1;
    }

    return this->fields[name].index;
}

StructField Struct::get_field_at(uint32_t index) {
    for (auto pair : this->fields) {
        if (pair.second.index == index) {
            return pair.second;
        }
    }

    return { "", nullptr, false, false, 0, 0 };
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

std::vector<utils::Ref<Struct>> Struct::expand() {
   std::vector<utils::Ref<Struct>> parents;
    for (auto parent : this->parents) {
        parents.push_back(parent);

        auto expanded = parent->expand();
        parents.insert(parents.end(), expanded.begin(), expanded.end());
    }

    return parents;
}
