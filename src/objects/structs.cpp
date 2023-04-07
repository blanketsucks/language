#include <quart/objects/structs.h>
#include <quart/objects/scopes.h>

Struct::Struct(
    std::string name,
    std::string qualified_name,
    bool opaque,
    llvm::StructType* type,
    std::map<std::string, StructField> fields
) : name(name), qualified_name(qualified_name), type(type), fields(fields), opaque(opaque) {
    this->scope = nullptr;
}

bool Struct::has_method(const std::string& name) { 
    return this->scope->functions.find(name) != this->scope->functions.end(); 
}

utils::Ref<Function> Struct::get_method(const std::string& name) {
    return this->scope->functions[name]; 
}

int Struct::get_field_index(const std::string& name) {
    if (this->fields.find(name) == this->fields.end()) {
        return -1;
    }

    return this->fields[name].index;
}

StructField Struct::get_field_at(uint32_t index) {
    for (auto& entry : this->fields) {
        if (entry.second.index == index) {
            return entry.second;
        }
    }

    return {};
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
