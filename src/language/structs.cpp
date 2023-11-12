#include <quart/language/structs.h>
#include <quart/language/scopes.h>

using namespace quart;

Struct::Struct(const std::string& name, quart::StructType* type, bool opaque) : name(name), type(type), opaque(opaque) {
    this->scope = nullptr;
}

Struct::Struct(
    const std::string& name, quart::StructType* type, std::map<std::string, StructField> fields, bool opaque
) : name(name), type(type), fields(fields), opaque(opaque) {
    this->scope = nullptr;
}

RefPtr<Struct> Struct::create(const std::string& name, quart::StructType* type, bool opaque) {
    return RefPtr<Struct>(new Struct(name, type, opaque));
}

RefPtr<Struct> Struct::create(
    const std::string& name, quart::StructType* type, std::map<std::string, StructField> fields, bool opaque
) {
    return RefPtr<Struct>(new Struct(name, type, fields, opaque));
}


bool Struct::has_method(const std::string& name) { 
    return this->scope->functions.find(name) != this->scope->functions.end(); 
}

RefPtr<Function> Struct::get_method(const std::string& name) { return this->scope->functions[name]; }

int Struct::get_field_index(const std::string& name) {
    if (this->fields.find(name) == this->fields.end()) {
        return -1;
    }

    return this->fields[name].index;
}

StructField Struct::get_field_at(u32 index) {
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
        if (pair.second.is_private()) {
            if (with_private) {
                fields.push_back(pair.second);
            }   
        } else {
            fields.push_back(pair.second);
        }
    }

    return fields;
}

std::vector<Struct*> Struct::expand() {
   std::vector<Struct*> parents;
    for (auto parent : this->parents) {
        parents.push_back(parent);

        auto expanded = parent->expand();
        parents.insert(parents.end(), expanded.begin(), expanded.end());
    }

    return parents;
}
