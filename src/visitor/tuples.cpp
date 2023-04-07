#include <quart/visitor.h>
#include <quart/utils/utils.h>

struct TupleElement {
    std::string name;
    llvm::Value* value;

    bool is_immutable;

    Span span;
};

bool Visitor::is_tuple(llvm::Type* type) {
    return type->isStructTy() && type->getStructName().startswith("__tuple");
}

llvm::StructType* Visitor::create_tuple_type(std::vector<llvm::Type*> types) {
    llvm::StructType* type = nullptr;
    if (this->tuples.find(types) == this->tuples.end()) {
        type = llvm::StructType::create(*this->context, types, "__tuple");
        this->tuples[types] = type;
    } else {
        type = this->tuples[types];
    }

    return type;
}

llvm::Value* Visitor::make_tuple(std::vector<llvm::Value*> values, llvm::StructType* type) {
    if (!type) {
        std::vector<llvm::Type*> types;
        for (auto& value : values) {
            types.push_back(value->getType());
        }

        type = this->create_tuple_type(types);
    }

    llvm::Value* tuple = llvm::UndefValue::get(type);
    for (size_t i = 0; i < values.size(); i++) {
        tuple = this->builder->CreateInsertValue(tuple, values[i], i);
    }

    return tuple;
}

Value Visitor::visit(ast::TupleExpr* expr) {
    std::vector<llvm::Type*> types;
    std::vector<llvm::Value*> elements;

    bool is_const = true;
    for (auto& elem : expr->elements) {
        Value val = elem->accept(*this);
        is_const &= val.is_constant;

        llvm::Value* value = val.unwrap(elem->span);

        elements.push_back(value);
        types.push_back(value->getType());
    }

    llvm::StructType* type = this->create_tuple_type(types);
    if (is_const) {
        std::vector<llvm::Constant*> constants;
        for (auto& element : elements) {
            constants.push_back(llvm::cast<llvm::Constant>(element));
        }

        return Value(llvm::ConstantStruct::get(type, constants), true);
    }

    if (!this->current_function) {
        ERROR(expr->span, "Tuple literals cannot contain non-constant elements");
    }

    return this->make_tuple(elements, type);
}  

void Visitor::store_tuple(
    Span span, 
    utils::Ref<Function> func, 
    llvm::Value* value, 
    const std::vector<ast::Ident>& names,
    std::string consume_rest
) {
    std::vector<llvm::Value*> values;
    if (consume_rest.empty()) {
        values = this->unpack(value, names.size(), span);
        for (auto& entry : utils::zip(names, values)) {
            llvm::AllocaInst* alloca = this->alloca(entry.second->getType());
            this->builder->CreateStore(entry.second, alloca);

            ast::Ident& ident = entry.first;
            func->scope->variables[ident.value] = Variable::from_alloca(
                ident.value, alloca, ident.is_immutable, ident.span
            );
        }

        return;
    }

    if (!consume_rest.empty() && names.size() == 1) {
        llvm::AllocaInst* alloca = this->alloca(value->getType());
        this->builder->CreateStore(value, alloca);

        func->scope->variables[consume_rest] = Variable::from_alloca(consume_rest, alloca);
        return;
    }

    // `let (foo, *bar, baz) = (1, 2, 3, 4, 5);`
    // here, foo takes the value of 1, baz 5 and bar is a tuple containing the rest of the elements.

    uint32_t n = 0;
    llvm::Type* vtype = value->getType();

    // TODO: Array support
    if (vtype->isPointerTy()) {
        llvm::Type* type = vtype->getPointerElementType();
        n = type->getStructNumElements();
    } else {
        n = vtype->getStructNumElements();
    }

    values = this->unpack(value, n, span);

    // We need to know how many variables are before and after the consume rest and it's position, from there
    // we can chop off the values and store them in their respective variables and the values left will be
    // stored in the consume rest variable as a tuple.
    auto iter = std::find_if(names.begin(), names.end(), [consume_rest](const ast::Ident& ident) {
        return ident.value == consume_rest;
    });

    size_t index = iter - names.begin();
    size_t rest = names.size() - index - 1;

    // We are forced to take elements from the back because we don't know how many elements would be in the rest tuple
    // We take as much values from the back as we have variables after the consume rest (in the case above it's 1)
    std::vector<llvm::Value*> last;
    for (size_t i = 0; i < rest; i++) {
        last.push_back(values.back());
        values.pop_back();
    }

    std::vector<TupleElement> finals;
    for (size_t i = 0; i < index; i++) {
        const ast::Ident& ident = names[i];

        finals.push_back({ident.value, values[i], ident.is_immutable, ident.span});
        values.erase(values.begin());
    }

    for (size_t i = index + 1; i < names.size(); i++) {
        const ast::Ident& ident = names[i];

        finals.push_back({ident.value, last.back(), ident.is_immutable, ident.span});
        last.pop_back();
    }

    // The rest of the elements remaining in `values` are the values needed in order to create the tuple for
    // `bar` in the example above

    std::vector<llvm::Type*> types;
    for (auto& value : values) {
        types.push_back(value->getType());
    }

    llvm::StructType* type = this->create_tuple_type(types);
    llvm::AllocaInst* alloca = this->alloca(type);

    for (size_t i = 0; i < values.size(); i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, alloca, i);
        this->builder->CreateStore(values[i], ptr);
    }

    func->scope->variables[consume_rest] = Variable::from_alloca(consume_rest, alloca);
    for (auto& entry : finals) {
        alloca = this->alloca(entry.value->getType());
        this->builder->CreateStore(entry.value, alloca);

        func->scope->variables[entry.name] = Variable::from_alloca(
            entry.name, alloca, entry.is_immutable, entry.span
        );
    }
}