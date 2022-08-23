#include "visitor.h"

Value Visitor::visit(ast::StructExpr* expr) {
    if (expr->opaque) {
        llvm::StructType* type = llvm::StructType::create(this->context, expr->name);
        
        this->structs[expr->name] = new Struct(expr->name, true, type, {});
        return this->constants["null"];
    }

    std::vector<Struct*> parents;
    std::vector<llvm::Type*> types;
    std::map<std::string, StructField> fields;

    // We do this to avoid an infinite loop when the struct has a field of the same type as the struct itself.
    llvm::StructType* type = llvm::StructType::create(this->context, {}, expr->name, expr->attributes.has("packed"));

    Struct* structure = new Struct(expr->name, false, type, {});
    this->structs[expr->name] = structure;

    for (auto& parent : expr->parents) {
        Value value = parent->accept(*this);
        if (!value.structure) {
            ERROR(parent->start, "Expected a structure");
        }

        std::vector<Struct*> expanded = value.structure->expand();
        parents.insert(parents.end(), expanded.begin(), expanded.end());

        structure->parents.push_back(value.structure);
        value.structure->children.push_back(structure);
    }

    if (this->current_namespace) {
        this->current_namespace->structs[expr->name] = structure;
    }

    for (auto& parent : parents) {
        for (auto& pair : parent->fields) {
            types.push_back(pair.second.type);
            fields[pair.first] = pair.second;
        }

        for (auto& pair : parent->methods) {
            structure->methods[pair.first] = pair.second;
        }
    }
    
    uint32_t index = fields.empty() ? 0 : fields.rbegin()->second.index + 1;
    uint32_t offset = 0;

    if (!fields.empty()) {
        StructField last = fields.rbegin()->second;
        offset = last.offset + this->getsizeof(last.type);
    }

    std::vector<ast::StructField> struct_fields = utils::values(expr->fields);
    std::sort(struct_fields.begin(), struct_fields.end(), [](auto& a, auto& b) { return a.index < b.index; });
    
    for (auto& field : struct_fields) {
        llvm::Type* ty = this->get_llvm_type(field.type);
        if (ty == type) {
            ERROR(expr->start, "Cannot have a field of the same type as the struct itself");
        }

        types.push_back(this->get_llvm_type(field.type));
    }

    type->setBody(types);
    for (auto& pair : utils::zip(struct_fields, types)) {
        ast::StructField field = pair.first;
        fields[field.name] = {field.name, pair.second, field.is_private, index, offset};

        index++;
        offset += this->getsizeof(pair.second);
    }

    structure->fields = fields;
    this->current_struct = structure;
    for (auto& method : expr->methods) { 
        assert(method->kind == ast::ExprKind::Function && "Expected a function. Might be a compiler bug.");
        method->accept(*this);
    }

    if (structure->has_method("constructor")) {
        Function* constructor = structure->methods["constructor"];
        if (constructor->ret != this->builder->getVoidTy()) {
            ERROR(expr->start, "Constructor must return void");
        }
    }

    this->current_struct = nullptr;
    return nullptr;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    llvm::Value* value = expr->parent->accept(*this).unwrap(this, expr->parent->start);
    llvm::Type* type = value->getType();

    bool is_pointer = false;
    if (!type->isStructTy()) {
        if (!(type->isPointerTy() && type->getPointerElementType()->isStructTy())) { 
            ERROR(expr->start, "Expected a struct or a pointer to a struct");
        }

        type = type->getPointerElementType();
        is_pointer = true;
    }

    std::string name = type->getStructName().str();
    Struct* structure = this->structs[name];

    if (structure->has_method(expr->attribute)) {
        Function* function = structure->methods[expr->attribute];
        if (this->current_struct != structure && function->is_private) {
            ERROR(expr->start, "Cannot access private method '{s}'", expr->attribute);
        }

        if (function->parent != structure) {
            Struct* parent = function->parent;
            value = this->builder->CreateBitCast(value, parent->type->getPointerTo());
        }

        if (is_pointer) {
            value = this->builder->CreateLoad(structure->type, value);
        }

        function->used = true;
        return Value(function->value, false, value, function);
    }

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", expr->attribute, name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->start, "Cannot access private field '{s}'", expr->attribute);
    }
    
    if (is_pointer) {
        llvm::Type* element = type->getStructElementType(index);

        llvm::Value* ptr = this->builder->CreateStructGEP(type, value, index);
        return this->builder->CreateLoad(element, ptr);
    } else {
        return this->builder->CreateExtractValue(value, index);
    }
}

Value Visitor::visit(ast::ConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.structure) {
        ERROR(expr->start, "Expected a struct");
    }

    Struct* structure = parent.structure;

    bool all_private = std::all_of(structure->fields.begin(), structure->fields.end(), [&](auto& pair) {
        return pair.second.is_private;
    });

    if (all_private && this->current_struct != structure) {
        ERROR(expr->start, "No public default constructor for struct '{s}'", structure->name);
    }

    bool is_const = std::all_of(expr->fields.begin(), expr->fields.end(), [](auto& pair) {
        return pair.second->is_constant();
    });

    std::map<int, llvm::Value*> args;
    int index = 0;

    for (auto& pair : expr->fields) {
        llvm::Value* value = pair.second->accept(*this).unwrap(this, pair.second->start);
        int i = index;
        if (!pair.first.empty()) {
            i = structure->get_field_index(pair.first);
            if (i < 0) {
                ERROR(pair.second->start, "Field '{s}' does not exist in struct '{s}'", pair.first, structure->name);
            } else if (args.find(i) != args.end()) {
                ERROR(pair.second->start, "Field '{s}' already initialized", pair.first);
            }

            StructField field = structure->fields.at(pair.first);
            if (this->current_struct != structure && field.is_private) {
                ERROR(pair.second->start, "Field '{s}' is private and cannot be initialized", pair.first);
            }
        }

        args[i] = value;
        index++;
    }

    std::vector<StructField> fields;
    if (this->current_struct == structure) {
        fields = structure->get_fields(true);
    } else {
        fields = structure->get_fields(false);
    }

    if (args.size() != fields.size()) {
        ERROR(expr->start, "Expected {i} fields, found {i}", fields.size(), args.size());
    }

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto& pair : args) {
            constants.push_back((llvm::Constant*)pair.second);
        }

        return Value(llvm::ConstantStruct::get(structure->type, constants), true);
    }
    
    llvm::Value* instance = this->builder->CreateAlloca(structure->type);
    for (auto& pair : args) {
        llvm::Value* pointer = this->builder->CreateStructGEP(structure->type, instance, pair.first);
        this->builder->CreateStore(pair.second, pointer);
    }

    return this->builder->CreateLoad(structure->type, instance);
}

std::pair<llvm::Value*, int> Visitor::get_struct_field(ast::AttributeExpr* expr) {
    llvm::Value* parent = expr->parent->accept(*this).unwrap(this, expr->start);
    llvm::Type* type = parent->getType();

    if (type->isStructTy()) {
        std::string name = type->getStructName().str();
        Struct* structure = this->structs[name];

        int index = structure->get_field_index(expr->attribute);
        if (index < 0) {
            ERROR(expr->start, "Field '{s}' does not exist in struct '{s}'", expr->attribute, name);
        }

        StructField field = structure->fields.at(expr->attribute);
        if (this->current_struct != structure && field.is_private) {
            ERROR(expr->start, "Cannot access private field '{s}'", expr->attribute);
        }

        return {parent, index};
    }

    if (!type->isPointerTy()) {
        ERROR(expr->start, "Attribute access on non-structure type");
    }

    type = type->getPointerElementType();
    if (!type->isStructTy()) {
        ERROR(expr->start, "Attribute access on non-structure type");
    }

    std::string name = type->getStructName().str();
    Struct* structure = this->structs.at(name);

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->start, "Attribute '{s}' does not exist in structure '{s}'", expr->attribute, name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->start, "Cannot access private field '{s}'", expr->attribute);
    }

    return {this->builder->CreateStructGEP(structure->type, parent, field.index), -1};
}