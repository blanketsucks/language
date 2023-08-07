// TODO: Rewrite when possible maybe?

#include <quart/visitor.h>

bool Visitor::is_struct(llvm::Value* value) {
    return this->is_struct(value->getType());
}

bool Visitor::is_struct(llvm::Type* type) {
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

    if (type->isPointerTy()) {
        type = type->getPointerElementType();
    }

    auto name = type->getStructName();
    if (name.empty()) {
        return nullptr;
    }

    return this->structs[name.str()];
}

utils::Ref<Struct> Visitor::make_struct(
    const std::string& name, const std::map<std::string, llvm::Type*>& fields
) {
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

    return this->structs[name];
}

llvm::StructType* Visitor::create_variadic_struct(llvm::Type* type) {
    if (this->variadics.find(type) != this->variadics.end()) {
        return this->variadics[type];
    }

    auto structure = this->make_struct(
        FORMAT("__variadic.{0}", this->id++),
        { 
            { "count", this->builder->getInt32Ty() },
            { "data", type->getPointerTo() }
        }
    );

    structure->scope = new Scope("variadic", ScopeType::Struct);
    this->scope->children.push_back(structure->scope);

    return structure->type;
}

Value Visitor::visit(ast::StructExpr* expr) {
    std::map<std::string, StructField> fields;
    if (expr->opaque) {
        std::string name = this->format_symbol(expr->name);
        llvm::StructType* type = llvm::StructType::create(*this->context, name);
        
        this->scope->structs[expr->name] = utils::make_ref<Struct>(
            expr->name, name, true, type, fields
        );

        return nullptr;
    }

    std::vector<utils::Ref<Struct>> parents;
    std::vector<llvm::Type*> types;
    utils::Ref<Struct> structure = nullptr;

    if (this->scope->structs.find(expr->name) == this->scope->structs.end()) {
        std::string name = this->format_symbol(expr->name);

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
                    StructField& field = fields[pair.first];

                    if (field.type != pair.second.type) {
                        ERROR(expr->span, "Field '{0}' has a different type than the same field in the parent structure", pair.first);
                    }
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
            Type ty = field.type->accept(*this);
            if (ty == type) {
                ERROR(expr->span, "Cannot define a field of the same type as the struct itself");
            } else if (!this->is_valid_sized_type(ty)) {
                ERROR(expr->span, "Cannot define a field of type '{0}'", this->get_type_name(ty));
            }

            if (fields.find(field.name) != fields.end()) {
                ERROR(expr->span, "Duplicate field '{0}'", field.name);
            }

            bool is_immutable = false;
            if (ty.is_pointer || ty.is_reference) {
                is_immutable = ty.is_immutable;
            }

            fields[field.name] = {
                field.name, 
                ty, 
                field.is_private, 
                field.is_readonly, 
                is_immutable,
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

    bool is_immutable = true;
    auto ref = this->as_reference(expr->parent);

    if (!ref.is_null()) {
        self = ref.value;
        type = ref.type->getPointerTo();

        is_immutable = ref.is_immutable;
    } else {
        self = expr->parent->accept(*this).unwrap(expr->parent->span);
        type = self->getType();
    }
    
    if (this->get_pointer_depth(type) > 1) {
        self = this->load(self);
        type = type->getPointerElementType();
    }

    if (!this->is_struct(type) && this->impls.find(type) == this->impls.end()) {
        ERROR(expr->parent->span, "Cannot access attribute of type '{0}'", this->get_type_name(type));
    }

    Scope* scope = nullptr;
    utils::Ref<Struct> structure = nullptr;
    
    if (this->is_struct(type)) {
        structure = this->get_struct(type);
        scope = structure->scope;
    } else {
        scope = this->impls[type].scope;
    }

    bool is_pointer = type->isPointerTy();
    if (scope->has_function(expr->attribute)) {
        if (!is_pointer) {
            llvm::Value* alloca = this->alloca(type);
            this->builder->CreateStore(self, alloca);

            self = alloca; type = type->getPointerTo();
        }

        auto function = scope->functions[expr->attribute];
        if (this->current_struct != structure && function->is_private) {
            ERROR(expr->parent->span, "Cannot access private method '{0}'", expr->attribute);
        }

        if (function->parent && function->parent != structure) {
            self = this->builder->CreateBitCast(self, function->parent->type->getPointerTo());
        }

        auto& arg = function->args[0];
        if (!arg.is_immutable && is_immutable) {
            NOTE(expr->parent->span, "Variable '{0}' is immutable", ref.name);
            ERROR(expr->span, "Cannot pass immutable reference to mutable argument '{0}'", arg.name);
        }

        if (arg.type != type) {
            llvm::Value* alloca = this->alloca(type);
            this->builder->CreateStore(self, alloca);

            self = alloca;
        }

        if (this->scope->has_variable(ref.name) && !arg.is_immutable) {
            this->mark_as_mutated(ref);
        }

        return Value::from_function(function, self);
    }

    if (!structure) {
        ERROR(expr->span, "Cannot access attribute '{0}' of type '{1}'", expr->attribute, this->get_type_name(type));
    }

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->span, "Field '{0}' does not exist in struct '{1}'", expr->attribute, structure->name);
    }

    StructField& field = structure->fields[expr->attribute];
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

    if (args.size() != structure->fields.size()) {
        for (auto& entry : structure->fields) {
            if (args.find(entry.second.index) == args.end()) {
                args[entry.second.index] = llvm::Constant::getNullValue(entry.second.type);
            }
        }
    }
    
    if (args.empty()) {
        return llvm::ConstantAggregateZero::get(structure->type);
    }

    if (is_const && !this->current_function) {
        std::vector<llvm::Constant*> values;
        for (auto& entry : args) {
            values.push_back(llvm::cast<llvm::Constant>(entry.second));
        }

        return Value(llvm::ConstantStruct::get(structure->type, values), true);
    }

    llvm::Value* alloca = this->alloca(structure->type);
    for (auto& arg : args) {
        llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, alloca, arg.first);
        this->builder->CreateStore(arg.second, ptr);
    }

    return Value::as_aggregate(alloca);
}

Value Visitor::visit(ast::EmptyConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!parent.structure) {
        ERROR(expr->span, "Expected a struct");
    }

    auto structure = parent.structure;
    std::vector<std::pair<llvm::Value*, int>> args;

    for (auto& field : structure->fields) {
        args.push_back({llvm::Constant::getNullValue(field.second.type), field.second.index});
    }

    if (args.empty()) {
        return llvm::ConstantAggregateZero::get(structure->type);
    }

    if (!this->current_function) {
        std::vector<llvm::Constant*> values;
        std::sort(args.begin(), args.end(), [](auto& a, auto& b) { return a.second < b.second; });
        
        for (auto& arg : args) {
            values.push_back(llvm::cast<llvm::Constant>(arg.first));
        }

        return Value(llvm::ConstantStruct::get(structure->type, values), true);
    }

    llvm::Value* alloca = this->alloca(structure->type);
    for (auto& arg : args) {
        llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, alloca, arg.second);
        this->builder->CreateStore(arg.first, ptr);
    }

    return Value::as_aggregate(alloca);

}

void Visitor::store_struct_field(ast::AttributeExpr* expr, utils::Scope<ast::Expr> value) {
    auto ref = this->as_reference(expr->parent);
    if (ref.is_null()) {
        llvm::Value* parent = expr->parent->accept(*this).unwrap(expr->parent->span);
        if (!parent->getType()->isPointerTy()) {
            if (!parent->getType()->isStructTy()) {
                ERROR(expr->parent->span, "Cannot access attribute of non-struct type '{0}'", this->get_type_name(parent->getType()));
            }

            ERROR(expr->span, "Cannot modify temporary struct value. Bind it to a variable first.");
        }

        ref.value = parent;
        ref.type = parent->getType()->getPointerElementType();
    }

    llvm::Value* parent = ref.value;
    if (!this->is_struct(ref.type)) {
        ERROR(expr->parent->span, "Cannot access attribute of non-struct type '{0}'", this->get_type_name(ref.type));
    }

    if (ref.is_immutable) {
        ERROR(expr->parent->span, "Cannot mutate immutable value '{0}'", ref.name);
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

    if (field.is_immutable) {
        ERROR(expr->span, "Cannot mutate immutable field '{0}'", expr->attribute);
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

    this->mark_as_mutated(ref);

    llvm::Value* ptr = this->builder->CreateStructGEP(structure->type, parent, field.index);
    this->builder->CreateStore(attr, ptr);
}