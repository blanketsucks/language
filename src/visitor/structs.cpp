#include "parser/ast.h"
#include "visitor.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/Constants.h"

Value Visitor::visit(ast::StructExpr* expr) {
    if (expr->opaque) {
        llvm::StructType* type = llvm::StructType::create(*this->context, expr->name);
        
        this->scope->structs[expr->name] = new Struct(expr->name, true, type, {});
        return this->constants["null"];
    }

    std::vector<Struct*> parents;
    std::vector<llvm::Type*> types;
    std::map<std::string, StructField> fields;

    Struct* structure = nullptr;
    bool exists = this->scope->structs.find(expr->name) != this->scope->structs.end();

    if (!exists) {
        // We do this to avoid an infinite loop when the struct has a field of the same type as the struct itself.
        std::string name = this->format_name(expr->name);
        llvm::StructType* type = llvm::StructType::create(*this->context, {}, name, expr->attributes.has("packed"));
        
        
        structure = new Struct(expr->name, false, type, {});

        this->type_to_struct[type] = structure;

        structure->start = expr->start;
        structure->end = expr->end;

        this->scope->structs[expr->name] = structure;
        this->structs[name] = structure;

        for (auto& parent : expr->parents) {
            Value value = parent->accept(*this);
            if (!value.structure) {
                utils::error(parent->start, "Expected a structure");
            }

            std::vector<Struct*> expanded = value.structure->expand();
            parents.insert(parents.end(), expanded.begin(), expanded.end());

            structure->parents.push_back(value.structure);
            value.structure->children.push_back(structure);
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
                utils::error(expr->start, "Cannot have a field of the same type as the struct itself");
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
    } else {
        if (expr->fields.size()) {
            utils::error(expr->start, "Re-definitions of structures must not define extra fields");
        }

        if (expr->parents.size()) {
            utils::error(expr->start, "Re-defintions of structures must not add new inheritance");
        }

        structure = this->scope->structs[expr->name];
    }
    
    this->current_struct = structure;
    for (auto& method : expr->methods) { 
        method->accept(*this);
    }

    this->current_struct = nullptr;
    return nullptr;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    Value parent = expr->parent->accept(*this);
    bool is_constant = parent.is_constant;

    llvm::Value* value = parent.unwrap(expr->parent->start);
    llvm::Type* type = value->getType();

    if (type->isStructTy()) {
        std::string name = type->getStructName().str();
        Struct* structure = this->structs[name];

        int index = structure->get_field_index(expr->attribute);
        if (index < 0) {
            ERROR(expr->parent->start, "Field '{0}' does not exist in struct '{1}'", expr->attribute, name);
        }

        StructField field = structure->fields.at(expr->attribute);
        if (this->current_struct != structure && field.is_private) {
            ERROR(expr->parent->start, "Cannot access private field '{0}'", expr->attribute);
        }
        
        return this->builder->CreateExtractValue(value, index);
    }

    type = type->getNonOpaquePointerElementType();
    if (!type->isStructTy()) {
        utils::error(expr->parent->start, "Expected a structure");
    }

    std::string name = type->getStructName().str();
    Struct* structure = this->structs[name];

    if (structure->has_method(expr->attribute)) {
        Function* function = structure->methods[expr->attribute];
        if (!value->getType()->isPointerTy()) {
            value = this->get_pointer_from_expr(std::move(expr->parent)).first;
        }

        if (this->current_struct != structure && function->is_private) {
            ERROR(expr->parent->start, "Cannot access private method '{0}'", expr->attribute);
        }

        if (function->parent != structure) {
            Struct* parent = function->parent;
            value = this->builder->CreateBitCast(value, parent->type->getPointerTo());
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
            utils::error(expr->start, "Expected a constant structure");
        } 

        llvm::ConstantStruct* structure = llvm::cast<llvm::ConstantStruct>(value);
        return Value(structure->getAggregateElement(index), true, nullptr);
    }

    llvm::Value* ptr = this->builder->CreateStructGEP(type, value, index);
    return this->load(ptr);
}

Value Visitor::visit(ast::ConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.structure) {
        utils::error(expr->start, "Expected a struct");
    }

    Struct* structure = parent.structure;

    bool all_private = std::all_of(structure->fields.begin(), structure->fields.end(), [&](auto& pair) {
        return pair.second.is_private;
    });

    if (all_private && this->current_struct != structure) {
        ERROR(expr->start, "No public default constructor for struct '{0}'", structure->name);
    }


    std::map<int, llvm::Value*> args;
    int index = 0;
    bool is_const = true;

    for (auto& pair : expr->fields) {
        int i = index;
        StructField field;

        if (!pair.first.empty()) {
            i = structure->get_field_index(pair.first);
            if (i < 0) {
                ERROR(pair.second->start, "Field '{0}' does not exist in struct '{1}'", pair.first, structure->name);
            } else if (args.find(i) != args.end()) {
                ERROR(pair.second->start, "Field '{0}' already initialized", pair.first);
            }

            field = structure->fields.at(pair.first);
            if (this->current_struct != structure && field.is_private) {
                ERROR(pair.second->start, "Field '{1}' is private and cannot be initialized", pair.first);
            }
        } else {
            field = structure->get_field_at(index);
        }

        this->ctx = field.type;
        Value val = pair.second->accept(*this);

        is_const &= val.is_constant;
        llvm::Value* value = val.unwrap(pair.second->start);

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

    llvm::Value* instance = llvm::UndefValue::get(structure->type);
    for (auto& arg : args) {
        instance = this->builder->CreateInsertValue(instance, arg.second, arg.first);
    }

    return instance;
}

void Visitor::store_struct_field(ast::AttributeExpr* expr, utils::Ref<ast::Expr> value) {
    llvm::Value* parent = this->get_pointer_from_expr(std::move(expr->parent)).first;
    llvm::Type* type = parent->getType();

    if (!type->isPointerTy()) {
        utils::error(expr->start, "Attribute access on non-structure type");
    }

    type = type->getNonOpaquePointerElementType();
    if (!type->isStructTy()) {
        utils::error(expr->start, "Attribute access on non-structure type");
    }

    std::string name = type->getStructName().str();
    Struct* structure = this->structs[name];

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->start, "Attribute '{0}' does not exist in structure '{1}'", expr->attribute, name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->start, "Cannot access private field '{0}'", expr->attribute);
    }

    llvm::Value* attr = value->accept(*this).unwrap(value->start);
    if (!this->is_compatible(attr->getType(), field.type)) {
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