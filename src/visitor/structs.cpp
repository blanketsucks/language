// TODO: Rewrite when possible

#include "visitor.h"

bool Visitor::is_struct(llvm::Value* value) {
    return this->is_struct(value->getType());
}

bool Visitor::is_struct(llvm::Type* type) {
    if (this->impls.find(type) != this->impls.end()) {
        return true;
    }

    if (!type->isStructTy()) {
        if (!type->isPointerTy()) {
            return false;
        }

        type = type->getPointerElementType();
        if (!type->isStructTy()) {
            return false;
        }
    }

    return true;
}

utils::Ref<Struct> Visitor::get_struct(llvm::Value* value) {
    return this->get_struct(value->getType());
}

utils::Ref<Struct> Visitor::get_struct(llvm::Type* type) {
    if (!this->is_struct(type)) {
        return nullptr;
    }

    if (this->impls.find(type) != this->impls.end()) {
        return this->impls[type];
    }

    if (type->isPointerTy()) {
        type = type->getPointerElementType();
    }

    auto name = type->getStructName();
    if (name.empty()) {
        return nullptr;
    }

    return this->structs[name.str()];
}

llvm::StructType* Visitor::make_struct(std::string name, std::map<std::string, llvm::Type*> fields) {
    std::map<std::string, StructField> sfields;
    std::vector<llvm::Type*> types;

    uint32_t index = 0;
    uint32_t offset = 0;

    for (auto& entry : fields) {
        sfields[entry.first] = StructField {
            entry.first,
            entry.second,
            false,
            false,
            index,
            offset
        };

        types.push_back(entry.second);

        index++;
        offset += this->getsizeof(entry.second);
    }

    llvm::StructType* type = llvm::StructType::create(*this->context, types, name, false);
    this->structs[name] = utils::make_ref<Struct>(
        name, name, false, type, sfields
    );

    return type;
}

llvm::StructType* Visitor::create_variadic_struct(llvm::Type* type) {
    if (this->variadics.find(type) != this->variadics.end()) {
        return this->variadics[type];
    }

    return this->make_struct(
        FORMAT("__variadic.{0}", this->id++),
        { 
            { "count", this->builder->getInt32Ty() },
            { "data", type->getPointerTo() }
        }
    );
}

Value Visitor::visit(ast::StructExpr* expr) {
    std::map<std::string, StructField> fields;
    if (expr->opaque) {
        std::string name = this->format_name(expr->name);
        llvm::StructType* type = llvm::StructType::create(*this->context, name);
        
        this->scope->structs[expr->name] = utils::make_ref<Struct>(
            expr->name, name, true, type, fields
        );

        return nullptr;
    }

    std::vector<utils::Ref<Struct>> parents;
    std::vector<llvm::Type*> types;
    utils::Ref<Struct> structure = nullptr;

    bool exists = this->scope->structs.find(expr->name) != this->scope->structs.end();
    if (!exists) {
        std::string name = this->format_name(expr->name);

        // We do this to avoid an infinite loop when the struct has a field of the same type as the struct itself.
        llvm::StructType* type = llvm::StructType::create(
            *this->context, 
            {}, 
            name, 
            expr->attributes.has(Attribute::Packed)
        );

        structure = utils::make_ref<Struct>(
            expr->name, name, false, type, fields
        );

        structure->span = expr->span;

        this->scope->structs[expr->name] = structure;
        this->structs[name] = structure;

        structure->scope = this->create_scope(name, ScopeType::Struct);
        for (auto& parent : expr->parents) {
            Value value = parent->accept(*this);
            if (!value.structure) {
                ERROR(parent->span, "Expected a structure");
            }

            auto expanded = value.structure->expand();
            expanded.insert(expanded.begin(), value.structure);

            parents.insert(parents.end(), expanded.begin(), expanded.end());

            structure->parents.push_back(value.structure);
            value.structure->children.push_back(structure);
        }

        for (auto& parent : parents) {
            for (auto& pair : parent->fields) {
                if (fields.find(pair.first) != fields.end()) {
                    continue; // Ignore for now, fix later
                }

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

        std::sort(
            expr->fields.begin(), expr->fields.end(), 
            [](auto& a, auto& b) { return a.index < b.index; }
        );

        for (auto& field : expr->fields) {
            llvm::Type* ty = field.type->accept(*this).type.value;
            if (ty == type) {
                ERROR(expr->span, "Cannot define a field of the same type as the struct itself");
            } else if (!this->is_valid_sized_type(ty)) {
                ERROR(expr->span, "Cannot define a field of type '{0}'", this->get_type_name(ty));
            }

            if (fields.find(field.name) != fields.end()) {
                ERROR(expr->span, "Duplicate field '{0}'", field.name);
            }

            fields[field.name] = {
                field.name, 
                ty, 
                field.is_private, 
                field.is_readonly, 
                index++, 
                offset,
            };

            offset += this->getsizeof(ty);
            types.push_back(ty);
        }

        type->setBody(types);
        structure->fields = fields;
    } else {
        if (expr->fields.size()) {
            ERROR(expr->span, "Re-definitions of structures must not define extra fields");
        }

        if (expr->parents.size()) {
            ERROR(expr->span, "Re-defintions of structures must not add new inheritance");
        }

        structure = this->scope->structs[expr->name];
        this->scope = structure->scope;
    }

    if (expr->attributes.has(Attribute::Impl)) {
        // TODO: [[impl(i32)]] struct Foo { ... }, Here whenever Foo is referenced inside the struct body it will reference the actual
        // struct type itself instead of the type that is being implemented. 
        // I don't know if i should keep it this way or change it so that it references the impl type.
        auto& impl = expr->attributes.get(Attribute::Impl);
        llvm::Type* type = impl.expr->accept(*this).type.value;

        if (type->isStructTy() || type->isArrayTy() || type->isFunctionTy()) {
            ERROR(impl.expr->span, "Cannot implement type '{0}'", this->get_type_name(type));
        }

        if (type->isPointerTy()) {
            llvm::Type* element = type->getPointerElementType();
            if (element->isStructTy() || element->isArrayTy() || element->isFunctionTy()) {
                ERROR(impl.expr->span, "Cannot implement type '{0}'", this->get_type_name(type));
            }
        }
        
        if (this->impls.find(type) != this->impls.end()) {
            ERROR(expr->span, "Cannot implement '{0}' for '{1}' because it is already implemented.", this->get_type_name(type));
        }

        if (!structure->fields.empty()) {
            ERROR(expr->span, "Cannot have fields in an implementation struct");
        }

        structure->impl = type;
        this->impls[type] = structure;
    }

    this->scope->structs["Self"] = structure;
    this->current_struct = structure;

    for (auto& method : expr->methods) { 
        method->accept(*this);
    }

    this->current_struct = nullptr;
    this->scope->exit(this);
    
    return nullptr;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    llvm::Value* self = nullptr;
    llvm::Type* type = nullptr;

    auto ref = this->as_reference(expr->parent);
    if (!ref.is_null()) {
        self = ref.value;
        type = ref.type->getPointerTo();
    } else {
        self = expr->parent->accept(*this).unwrap(expr->parent->span);
        type = self->getType();
    }

    if (!this->is_struct(type)) {
        ERROR(expr->parent->span, "Cannot access attribute of non-struct type '{0}'", this->get_type_name(type));
    }

    if (this->get_pointer_depth(type) > 1) {
        self = this->builder->CreateLoad(type, self);
        type = type->getPointerElementType();
    }

    bool is_pointer = type->isPointerTy();
    auto structure = this->get_struct(type);

    if (structure->scope) {
        if (structure->scope->has_function(expr->attribute)) {
            if (!is_pointer) {
                llvm::Value* tmp = this->as_reference(self);
                if (!tmp) {
                    if (!structure->impl) {
                        ERROR(expr->parent->span, "Cannot access method '{0}' of a temporary structure", expr->attribute);
                    }
                    
                    llvm::Value* alloca = this->create_alloca(structure->impl);
                    this->builder->CreateStore(self, alloca);
                    
                    self = alloca;
                } else {
                    self = tmp;
                }
            }

            auto function = structure->scope->functions[expr->attribute];
            if (this->current_struct != structure && function->is_private) {
                ERROR(expr->parent->span, "Cannot access private method '{0}'", expr->attribute);
            }

            if (function->parent && function->parent != structure) {
                self = this->builder->CreateBitCast(self, function->parent->type->getPointerTo());
            }

            return Value::from_function(function, self);
        }
    }

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->span, "Field '{0}' does not exist in struct '{1}'", expr->attribute, structure->name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->span, "Cannot access private field '{0}'", expr->attribute);
    }
    
    if (is_pointer) {
        return this->load(this->builder->CreateStructGEP(
            type->getPointerElementType(), self, index
        ));
    }

    return this->builder->CreateExtractValue(self, index);
}

Value Visitor::visit(ast::ConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.structure) {
        ERROR(expr->span, "Expected a struct");
    }

    auto structure = parent.structure;
    bool all_private = std::all_of(structure->fields.begin(), structure->fields.end(), [&](auto& pair) {
        return pair.second.is_private;
    });

    if (all_private && this->current_struct != structure) {
        ERROR(expr->span, "No public default constructor for struct '{0}'", structure->name);
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
                ERROR(entry.value->span, "Field '{0}' does not exist in struct '{1}'", entry.name, structure->name);
            } else if (args.find(i) != args.end()) {
                ERROR(entry.value->span, "Field '{0}' already initialized", entry.name);
            }

            field = structure->fields.at(entry.name);
            if (this->current_struct != structure && field.is_private) {
                ERROR(entry.value->span, "Field '{1}' is private and cannot be initialized", entry.name);
            }
        } else {
            field = structure->get_field_at(index);
        }

        this->ctx = field.type;
        Value val = entry.value->accept(*this);

        is_const &= val.is_constant;
        llvm::Value* value = val.unwrap(entry.value->span);

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
        ERROR(expr->span, "Expected {0} fields, found {1}", fields.size(), args.size());
    }

    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto& pair : args) {
            constants.push_back(llvm::cast<llvm::Constant>(pair.second));
        }

        return Value(llvm::ConstantStruct::get(structure->type, constants), true);
    }
    
    if (args.empty()) {
        return llvm::ConstantAggregateZero::get(structure->type);
    }

    llvm::Value* alloca = this->create_alloca(structure->type);
    for (auto& arg : args) {
        llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, alloca, arg.first);
        this->builder->CreateStore(arg.second, ptr);
    }

    return this->load(alloca);
}

void Visitor::store_struct_field(ast::AttributeExpr* expr, utils::Scope<ast::Expr> value) {
    auto ref = this->as_reference(expr->parent);
    llvm::Value* parent = ref.value;

    if (!this->is_struct(ref.type)) {
        ERROR(expr->parent->span, "Cannot access attribute of non-struct type '{0}'", this->get_type_name(ref.type));
    }

    if (ref.is_immutable) {
        ERROR(expr->parent->span, "Cannot modify immutable value '{0}'", ref.name);
    }

    if (ref.type->isPointerTy()) {
        parent = this->load(parent);
    }

    auto structure = this->get_struct(ref.type);
    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->span, "Attribute '{0}' does not exist in structure '{1}'", expr->attribute, name);
    }

    StructField field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private) {
        ERROR(expr->span, "Cannot access private field '{0}'", expr->attribute);
    }

    if (this->current_struct != structure && field.is_readonly) {
        ERROR(expr->span, "Cannot modify readonly field '{0}'", expr->attribute);
    }

    llvm::Value* attr = value->accept(*this).unwrap(value->span);
    if (!this->is_compatible(field.type, attr->getType())) {
        ERROR(
            value->span, 
            "Cannot assign value of type '{0}' to type '{1}' for struct field '{2}'", 
            this->get_type_name(attr->getType()), this->get_type_name(field.type), field.name
        );
    } else {
        attr = this->cast(attr, field.type);
    }

    llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, parent, field.index);
    this->builder->CreateStore(attr, ptr);
}