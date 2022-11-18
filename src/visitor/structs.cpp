#include "visitor.h"
#include <stdint.h>

Value Visitor::visit(ast::StructExpr* expr) {
    std::map<std::string, StructField> fields;
    if (expr->opaque) {
        std::string name = this->format_name(expr->name);
        llvm::StructType* type = llvm::StructType::create(*this->context, name);
        
        this->scope->structs[expr->name] = utils::make_shared<Struct>(
            expr->name, name, true, type, fields
        );

        return nullptr;
    }

    std::vector<utils::Shared<Struct>> parents;
    std::vector<llvm::Type*> types;
    utils::Shared<Struct> structure = nullptr;

    bool exists = this->scope->structs.find(expr->name) != this->scope->structs.end();
    if (!exists) {
        std::string name = this->format_name(expr->name);

        // We do this to avoid an infinite loop when the struct has a field of the same type as the struct itself.
        llvm::StructType* type = llvm::StructType::create(*this->context, {}, name, expr->attributes.has("packed"));
        structure = utils::make_shared<Struct>(
            expr->name, name, false, type, fields
        );

        structure->start = expr->start;
        structure->end = expr->end;

        this->scope->structs[expr->name] = structure;
        this->structs[name] = structure;

        structure->scope = this->create_scope(name, ScopeType::Struct);
        for (auto& parent : expr->parents) {
            Value value = parent->accept(*this);
            if (!value.structure) {
                ERROR(parent->start, "Expected a structure");
            }

            auto expanded = value.structure->expand();
            expanded.insert(expanded.begin(), value.structure);

            parents.insert(parents.end(), expanded.begin(), expanded.end());

            structure->parents.push_back(value.structure);
            value.structure->children.push_back(structure);
        }

        for (auto& parent : parents) {
            for (auto& pair : parent->fields) {
                types.push_back(pair.second.type);
                fields[pair.first] = pair.second;
            }

            for (auto& pair : parent->scope->functions) {
                structure->scope->functions[pair.first] = pair.second;
            }
        }
        
        uint32_t index = fields.empty() ? 0 : fields.rbegin()->second.index + 1;
        uint32_t offset = 0;

        if (!fields.empty()) {
            StructField last = fields.rbegin()->second;
            offset = last.offset + this->getsizeof(last.type);
        }

        std::vector<ast::StructField> sfields;
        for (auto& entry : expr->fields) {
            sfields.push_back(std::move(entry.second));
        }

        std::sort(sfields.begin(), sfields.end(), [](auto& a, auto& b) { return a.index < b.index; });        
        for (auto& field : sfields) {
            llvm::Type* ty = field.type->accept(*this).type;
            if (ty == type) {
                ERROR(expr->start, "Cannot have a field of the same type as the struct itself");
            }

            fields[field.name] = {field.name, ty, field.is_private, index, offset};
            index++;

            offset += this->getsizeof(ty);
            types.push_back(ty);
        }

        type->setBody(types);
        structure->fields = fields;
    } else {
        if (expr->fields.size()) {
            ERROR(expr->start, "Re-definitions of structures must not define extra fields");
        }

        if (expr->parents.size()) {
            ERROR(expr->start, "Re-defintions of structures must not add new inheritance");
        }

        structure = this->scope->structs[expr->name];
        this->scope = structure->scope;
    }
    
    this->current_struct = structure;
    for (auto& method : expr->methods) { 
        method->accept(*this);
    }

    this->current_struct = nullptr;
    this->scope->exit(this);

    return nullptr;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    Value parent = expr->parent->accept(*this);
    bool is_constant = parent.is_constant;

    llvm::Value* value = parent.unwrap(expr->parent->start);
    llvm::Type* type = value->getType();

    std::string name;
    bool is_pointer = false;

    if (type->isStructTy()) {
        name = type->getStructName().str();
    } else if (type->isPointerTy()) {
        llvm::Type* element = type->getNonOpaquePointerElementType();
        if (element->isStructTy()) {
            name = element->getStructName().str();
            is_pointer = true;
        }
    }

    if (name.empty()) {
        ERROR(expr->start, "Cannot access attributes of non-struct types");
    }

    auto structure = this->structs[name];
    if (structure->scope->has_function(expr->attribute)) {
        auto function = structure->scope->functions[expr->attribute];
        if (!is_pointer) {
            value = this->get_pointer_from_expr(expr->parent.get()).first;
        }

        if (this->current_struct != structure && function->is_private) {
            ERROR(expr->parent->start, "Cannot access private method '{0}'", expr->attribute);
        }
        
        function->used = true;
        return Value(function->value, false, value, function);
    }

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->start, "Field '{0}' does not exist in struct '{1}'", expr->attribute, name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->start, "Cannot access private field '{0}'", expr->attribute);
    }
    
    if (is_constant) {
        if (!llvm::isa<llvm::ConstantStruct>(value)) {
            ERROR(expr->start, "Expected a constant structure");
        } 

        llvm::ConstantStruct* structure = llvm::cast<llvm::ConstantStruct>(value);
        return Value(structure->getAggregateElement(index), true);
    } else if (!is_pointer) {
        return this->builder->CreateExtractValue(value, index);
    }

    llvm::Value* ptr = this->builder->CreateStructGEP(type->getNonOpaquePointerElementType(), value, index);
    return this->load(ptr);
}

Value Visitor::visit(ast::ConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.structure) {
        ERROR(expr->start, "Expected a struct");
    }

    auto structure = parent.structure;
    bool all_private = std::all_of(structure->fields.begin(), structure->fields.end(), [&](auto& pair) {
        return pair.second.is_private;
    });

    if (all_private && this->current_struct != structure) {
        ERROR(expr->start, "No public default constructor for struct '{0}'", structure->name);
    }

    std::map<int, llvm::Value*> args;
    int index = 0;
    bool is_const = true;

    for (auto& entry : expr->fields) {
        int i = index;
        StructField field;

        if (!entry.name.empty()) {
            i = structure->get_field_index(entry.name);
            if (i < 0) {
                ERROR(entry.value->start, "Field '{0}' does not exist in struct '{1}'", entry.name, structure->name);
            } else if (args.find(i) != args.end()) {
                ERROR(entry.value->start, "Field '{0}' already initialized", entry.name);
            }

            field = structure->fields.at(entry.name);
            if (this->current_struct != structure && field.is_private) {
                ERROR(entry.value->start, "Field '{1}' is private and cannot be initialized", entry.name);
            }
        } else {
            field = structure->get_field_at(index);
        }

        this->ctx = field.type;
        Value val = entry.value->accept(*this);

        is_const &= val.is_constant;
        llvm::Value* value = val.unwrap(entry.value->start);

        args[i] = value;
        index++;

        this->ctx = nullptr;
    }

    std::vector<StructField> fields;
    if (this->current_struct == structure) {
        fields = structure->get_fields(true);
    } else {
        fields = structure->get_fields(false);
    }

    if (args.size() != fields.size()) {
        ERROR(expr->start, "Expected {0} fields, found {1}", fields.size(), args.size());
    }

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto& pair : args) {
            constants.push_back(llvm::cast<llvm::Constant>(pair.second));
        }

        return Value(llvm::ConstantStruct::get(structure->type, constants), true);
    }
    
    if (!args.size()) {
        return llvm::ConstantAggregateZero::get(structure->type);
    }

    llvm::Value* instance = llvm::ConstantAggregateZero::get(structure->type);
    for (auto& arg : args) {
        instance = this->builder->CreateInsertValue(instance, arg.second, arg.first);
    }

    return instance;
}

void Visitor::store_struct_field(ast::AttributeExpr* expr, utils::Ref<ast::Expr> value) {
    llvm::Value* parent = this->get_pointer_from_expr(expr->parent.get()).first;
    llvm::Type* type = parent->getType();

    if (!type->isPointerTy()) {
        ERROR(expr->start, "Attribute access on non-structure type");
    }

    type = type->getNonOpaquePointerElementType();
    if (type->isPointerTy()) {
        type = type->getNonOpaquePointerElementType();
        parent = this->load(parent);
    }

    if (!type->isStructTy()) {
        ERROR(expr->start, "Attribute access on non-structure type");
    }

    std::string name = type->getStructName().str();
    auto structure = this->structs[name];

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->start, "Attribute '{0}' does not exist in structure '{1}'", expr->attribute, name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->start, "Cannot access private field '{0}'", expr->attribute);
    }

    llvm::Value* attr = value->accept(*this).unwrap(value->start);
    if (!this->is_compatible(field.type, attr->getType())) {
        ERROR(
            value->start, 
            "Cannot assign value of type '{0}' to type '{1}' for struct field '{2}'", 
            this->get_type_name(attr->getType()), this->get_type_name(field.type), field.name
        );
    } else {
        attr = this->cast(attr, field.type);
    }

    llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, parent, field.index);
    this->builder->CreateStore(attr, ptr);
}