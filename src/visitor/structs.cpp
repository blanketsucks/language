// TODO: Rewrite when possible maybe?

#include <quart/visitor.h>

using namespace quart;

RefPtr<Struct> Visitor::get_struct_from_type(quart::Type* type) {
    if (type->is_pointer()) type = type->get_pointee_type();
    if (!type->is_struct()) return nullptr;

    return this->structs[type];
}

RefPtr<Struct> Visitor::make_struct(
    const std::string& name, const std::map<std::string, quart::Type*>& fields
) {
    std::map<std::string, StructField> sfields;

    std::vector<Type*> types;
    std::vector<llvm::Type*> llvm_types;

    u32 index = 0;
    u32 offset = 0;

    for (auto& entry : fields) {
        sfields[entry.first] = StructField {
            entry.first,
            entry.second,
            StructField::None,
            index,
            offset
        };

        types.push_back(entry.second);

        llvm::Type* type = entry.second->to_llvm_type();
        llvm_types.push_back(type);

        index++;
        offset += this->getsizeof(type);
    }

    quart::StructType* type = this->registry->create_struct_type(
        name, 
        types,
        llvm::StructType::create(*this->context, llvm_types, name, false)
    );

    auto structure = Struct::create(name, type, sfields, false);
    this->structs[type] = structure;
    
    return structure;
}

Value Visitor::create_struct_value(Struct& structure, const std::vector<llvm::Value*>& args) {
    llvm::AllocaInst* alloca = this->alloca(structure.type->to_llvm_type());
    for (auto& entry : llvm::enumerate(args)) {
        llvm::Value* ptr = this->builder->CreateStructGEP(structure.type->to_llvm_type(), alloca, entry.index());
        this->builder->CreateStore(entry.value(), ptr);
    }

    return { alloca, structure.type, Value::Aggregate };
}

Value Visitor::visit(ast::StructExpr* expr) {
    std::map<std::string, StructField> fields;
    if (expr->opaque) {
        std::string name = this->format_symbol(expr->name);
        quart::StructType* type = this->registry->create_struct_type(
            name, {}, llvm::StructType::create(*this->context, name)
        );
        
        this->scope->structs[expr->name] = Struct::create(name, type, true);
        return EMPTY_VALUE;
    }

    std::vector<quart::Type*> types;
    RefPtr<Struct> structure = nullptr;

    if (this->scope->structs.find(expr->name) == this->scope->structs.end()) {
        std::string name = this->format_symbol(expr->name);

        std::vector<llvm::Type*> llvm_types;
        quart::StructType* type = this->registry->create_struct_type(
            name, 
            {}, 
            llvm::StructType::create(*this->context, {}, name, false)
        );

        structure = Struct::create(expr->name, type, false);
        structure->span = expr->span;

        this->scope->structs[expr->name] = structure;
        this->structs[type] = structure;

        Scope* scope = Scope::create(name, ScopeType::Struct, this->scope);
        structure->scope = scope;
        
        u32 index = fields.empty() ? 0 : fields.rbegin()->second.index + 1;
        u32 offset = 0;

        if (!fields.empty()) {
            StructField last = fields.rbegin()->second;
            offset = last.offset + this->getsizeof(last.type);
        }

        llvm::sort(expr->fields, [](auto& a, auto& b) { return a.index < b.index; });
        for (auto& field : expr->fields) {
            quart::Type* ty = field.type->accept(*this);
            if (ty == type) {
                ERROR(field.type->span, "Cannot define a field of the same type as the struct itself");
            } else if (!ty->is_sized_type()) {
                ERROR(field.type->span, "Cannot define a field of type '{0}'", ty->get_as_string());
            }

            if (fields.find(field.name) != fields.end()) {
                ERROR(expr->span, "Duplicate field '{0}'", field.name);
            }

            u8 flags = StructField::None;
            if (field.is_private) flags |= StructField::Private;
            if (field.is_readonly) flags |= StructField::Readonly;
            if (ty->is_mutable()) flags |= StructField::Mutable;

            fields[field.name] = StructField {
                field.name, 
                ty,
                flags, 
                index++, 
                offset
            };

            offset += this->getsizeof(ty);

            llvm_types.push_back(ty->to_llvm_type());
            types.push_back(ty);
        }

        type->set_fields(types);
        llvm::StructType* stype = type->get_llvm_struct_type();

        stype->setBody(llvm_types);
        structure->fields = std::move(fields);
    } else {
        if (expr->fields.size()) {
            ERROR(expr->span, "Re-definitions of structures must not define extra fields");
        }

        structure = this->scope->structs[expr->name];
    }

    this->push_scope(structure->scope);

    this->scope->structs["Self"] = structure;
    this->current_struct = structure;

    for (auto& method : expr->methods) { 
        method->accept(*this);
    }

    this->current_struct = nullptr;
    this->pop_scope();
    
    return nullptr;
}

Value Visitor::visit(ast::AttributeExpr* expr) {
    llvm::Value* self = nullptr;
    quart::Type* type = nullptr;

    bool is_mutable = true;

    auto ref = this->as_reference(*expr->parent);
    if (!ref.is_null()) {
        is_mutable = ref.flags & ScopeLocal::Mutable;

        self = ref.value;
        type = ref.type->get_pointer_to(is_mutable);
    } else {
        Value value = expr->parent->accept(*this);
        if (value.is_empty_value()) ERROR(expr->parent->span, "Expected a value");

        self = value.inner;
        type = value.type;
    }
    
    if (type->get_pointer_depth() > 1) {
        self = this->load(self);
        type = type->get_pointee_type();
    }

    bool is_struct_type = type->is_struct();
    if (!is_struct_type && type->is_pointer()) {
        quart::Type* ty = type->get_pointee_type();
        is_struct_type = ty->is_struct();
    }

    bool has_impl = this->impls.find(type) != this->impls.end();
    if (!is_struct_type && !has_impl) {
        ERROR(expr->parent->span, "Cannot access attribute of type '{0}'", type->get_as_string());
    }

    Scope* scope = nullptr;
    Struct* structure = nullptr;
    
    if (is_struct_type) {
        structure = this->get_struct_from_type(type).get();
        scope = structure->scope;
    } else {
        quart::Type* lookup = ref.is_null() ? type : type->get_pointee_type();
        scope = this->impls[lookup].scope;
    }

    if (scope->has_function(expr->attribute)) {
        if (!type->is_pointer()) {
            llvm::Value* alloca = this->alloca(type->to_llvm_type());
            this->builder->CreateStore(self, alloca);

            self = alloca; type = type->get_pointer_to(true);
        }

        auto function = scope->functions[expr->attribute];
        if (this->current_struct.get() != structure && function->is_private()) {
            ERROR(expr->parent->span, "Cannot access private method '{0}'", expr->attribute);
        }

        if (function->parent && function->parent != structure) {
            llvm::Type* type = function->parent->type->get_pointer_to(false)->to_llvm_type();
            self = this->builder->CreateBitCast(self, type);
        }

        auto& param = function->params[0];
        if (param.is_mutable() && !is_mutable) {
            NOTE(expr->parent->span, "Variable '{0}' is immutable", ref.name);
            ERROR(expr->span, "Cannot pass immutable reference to mutable argument '{0}'", param.name);
        }

        if (this->scope->has_variable(ref.name) && param.is_mutable()) {
            this->mark_as_mutated(ref);
        }

        return { function->value, function->type, Value::Function, function.get(), self };
    }

    if (!structure) {
        ERROR(expr->span, "Cannot access attribute '{0}' of type '{1}'", expr->attribute, type->get_as_string());
    }

    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->span, "Field '{0}' does not exist in struct '{1}'", expr->attribute, structure->name);
    }

    StructField& field = structure->fields[expr->attribute];
    if (this->current_struct.get() != structure && field.is_private()) {
        ERROR(expr->span, "Cannot access private field '{0}'", expr->attribute);
    }

    quart::Type* result = field.type;
    if (field.is_mutable() && !is_mutable) {
        if (result->is_reference()) {
            // &mut i32 -> &i32
            result = result->get_reference_type()->get_reference_to(false);
        } else if (result->is_pointer()) {
            // *mut i32 -> *i32
            result = result->get_pointee_type()->get_pointer_to(false);
        }
    }

    if (type->is_pointer()) {
        llvm::Type* llvm_type = type->to_llvm_type()->getPointerElementType();
        llvm::Value* ptr = this->builder->CreateStructGEP(llvm_type, self, index);

        return {this->load(ptr), result};
    }

    return {this->builder->CreateExtractValue(self, index), field.type};
}

Value Visitor::visit(ast::ConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!(parent.flags & Value::Struct)) {
        ERROR(expr->parent->span, "Expected a struct");
    }

    auto structure = parent.as<Struct*>();
    bool all_private = llvm::all_of(structure->fields, [](const auto& pair) {
        return pair.second.is_private();
    });

    if (all_private && this->current_struct.get() != structure) {
        ERROR(expr->span, "No public default constructor for struct '{0}'", structure->name);
    }

    std::map<u32, llvm::Value*> args;
    bool is_constant_value = true;

    for (auto& entry : expr->fields) {
        i32 index = structure->get_field_index(entry.name);
        if (index < 0) {
            ERROR(entry.value->span, "Field '{0}' does not exist in struct '{1}'", entry.name, structure->name);
        }

        StructField& field = structure->fields.at(entry.name);
        this->inferred = field.type;

        Value value = entry.value->accept(*this);
        if (value.is_empty_value()) ERROR(entry.value->span, "Expected a value");

        if (!Type::can_safely_cast_to(value.type, field.type)) {
            ERROR(
                entry.value->span, 
                "Cannot assign value of type '{0}' to type '{1}' for field '{2}'", 
                value.type->get_as_string(), field.type->get_as_string(), field.name
            );
        }

        value = this->cast(value, field.type);

        is_constant_value &= value.flags & Value::Constant;
        args[index] = value.inner;

        this->inferred = nullptr;
    }

    std::vector<StructField> fields;
    if (this->current_struct.get() == structure) {
        fields = structure->get_fields(true);
    } else {
        fields = structure->get_fields(false);
    }

    if (args.size() != fields.size()) {
        ERROR(expr->span, "Expected {0} fields, found {1}", fields.size(), args.size());
    }

    if (args.size() != structure->fields.size()) {
        for (auto& entry : structure->fields) {
            StructField& field = entry.second;

            auto value = args[field.index];
            if (value) continue;

            args[field.index] = llvm::Constant::getNullValue(field.type->to_llvm_type());
        }
    }
    
    llvm::Type* type = structure->type->to_llvm_type();
    if (is_constant_value && !this->current_function) {
        auto range = llvm::map_range(
            args, [](const auto& entry) { return llvm::cast<llvm::Constant>(entry.second); }
        );

        std::vector<llvm::Constant*> values(range.begin(), range.end());
        llvm::Constant* constant = llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(type), values);

        return { constant, structure->type, Value::Constant };
    }

    auto range = llvm::map_range(args, [](const auto& entry) { return entry.second; });
    return this->create_struct_value(*structure, { range.begin(), range.end() });
}

Value Visitor::visit(ast::EmptyConstructorExpr* expr) {
    Value parent = expr->parent->accept(*this);
    if (!(parent.flags & Value::Struct)) ERROR(expr->span, "Expected a struct");

    auto structure = parent.as<Struct*>();
    std::vector<llvm::Value*> args;

    for (auto& entry : structure->fields) {
        llvm::Type* type = entry.second.type->to_llvm_type();
        args.push_back(llvm::Constant::getNullValue(type));
    }

    llvm::Type* type = structure->type->to_llvm_type();
    if (args.empty()) {
        return {llvm::ConstantAggregateZero::get(type), structure->type, Value::Constant};
    }

    if (!this->current_function) {
        std::vector<llvm::Constant*> values;
        values.reserve(args.size());

        for (auto& arg : args) {
            values.push_back(llvm::cast<llvm::Constant>(arg));
        }

        llvm::Constant* constant = llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(type), values);
        return { constant, structure->type, Value::Constant };
    }

    return this->create_struct_value(*structure, args);
}

Value Visitor::evaluate_attribute_assignment(ast::AttributeExpr* expr, ast::Expr& v) {
    auto ref = this->as_reference(*expr->parent);
    if (!ref.is_mutable()) {
        ERROR(expr->parent->span, "Cannot mutate immutable variable '{0}'", ref.name);
    }

    quart::Type* type = ref.type;
    llvm::Value* parent = ref.value;

    if (ref.is_null()) {
        Value value = expr->parent->accept(*this);
        if (value.is_empty_value()) ERROR(expr->parent->span, "Expected a value");

        if (!value.type->is_pointer()) {
            ERROR(
                expr->parent->span, 
                "Cannot mutate temporary value of type '{0}'",
                value.type->get_as_string()
            );
        }

        parent = value.inner;
        type = value.type->get_pointee_type();

        if (!type->is_struct()) {
            ERROR(expr->parent->span, "Cannot access attribute of type '{0}'", type->get_as_string());
        }
    }

    if (type->get_pointer_depth() >= 1) {
        parent = this->load(parent);
        type = type->get_pointee_type();
    }

    bool is_struct = type->is_struct();
    if (!is_struct && type->is_pointer()) {
        type = type->get_pointee_type();
        is_struct = type->is_struct();
    }

    if (!is_struct) {
        ERROR(expr->parent->span, "Cannot access attribute of type '{0}'", type->get_as_string());
    }

    auto structure = this->get_struct_from_type(ref.type);
    int index = structure->get_field_index(expr->attribute);
    if (index < 0) {
        ERROR(expr->span, "Attribute '{0}' does not exist in structure '{1}'", expr->attribute, name);
    }

    StructField& field = structure->fields.at(expr->attribute);
    if (this->current_struct != structure && field.is_private()) {
        ERROR(expr->span, "Cannot access private field '{0}'", expr->attribute);
    }

    if (this->current_struct != structure && field.is_readonly()) {
        ERROR(expr->span, "Cannot modify readonly field '{0}'", expr->attribute);
    }

    if (!field.is_mutable()) {
        ERROR(expr->span, "Cannot mutate immutable field '{0}'", expr->attribute);
    }

    this->inferred = field.type;

    Value value = v.accept(*this);
    if (value.is_empty_value()) ERROR(v.span, "Expected a value");

    this->inferred = nullptr;
    if (!Type::can_safely_cast_to(value.type, field.type)) {
        ERROR(
            v.span, 
            "Cannot assign value of type '{0}' to type '{1}' for field '{2}'", 
            value.type->get_as_string(), field.type->get_as_string(), field.name
        );
    }

    value = this->cast(value, field.type);
    this->mark_as_mutated(ref);

    llvm::Value* ptr = this->builder->CreateStructGEP(structure->type->to_llvm_type(), parent, field.index);
    this->builder->CreateStore(value, ptr);

    return EMPTY_VALUE;
}