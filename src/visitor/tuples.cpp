#include "visitor.h"
#include "utils/utils.h"

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

        llvm::Value* value = val.unwrap(elem->start);

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
        ERROR(expr->start, "Tuple literals cannot contain non-constant elements");
    }

    return this->make_tuple(elements, type);
}  

void Visitor::store_tuple(
    Location location, 
    utils::Shared<Function> func, 
    llvm::Value* value, 
    std::vector<std::string> names,
    std::string consume_rest
) {
    std::vector<llvm::Value*> values;
    if (consume_rest.empty()) {
        values = this->unpack(value, names.size(), location);
        for (auto& pair : utils::zip(names, values)) {
            llvm::AllocaInst* alloca = this->create_alloca(pair.second->getType());
            this->builder->CreateStore(pair.second, alloca);

            func->scope->variables[pair.first] = Variable::from_alloca(pair.first, alloca);
        }

        return;
    }

    if (!consume_rest.empty() && names.size() == 1) {
        llvm::AllocaInst* alloca = this->create_alloca(value->getType());
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
        llvm::Type* type = vtype->getNonOpaquePointerElementType();
        n = type->getStructNumElements();
    } else {
        n = vtype->getStructNumElements();
    }

    values = this->unpack(value, n, location);

    // We need to know how many variables are before and after the consume rest and it's position, from there
    // we can chop off the values and store them in their respective variables and the values left will be
    // stored in the consume rest variable as a tuple.
    auto iter = std::find(names.begin(), names.end(), consume_rest);

    size_t index = iter - names.begin();
    size_t rest = names.size() - index - 1;

    // We are forced to take elements from the back because we don't know how many elements would be in the rest tuple
    // We take as much values from the back as we have variables after the consume rest (in the case above it's 1)
    std::vector<llvm::Value*> last;
    for (size_t i = 0; i < rest; i++) {
        last.push_back(values.back());
        values.pop_back();
    }

    std::vector<std::pair<std::string, llvm::Value*>> finals;
    for (size_t i = 0; i < index; i++) {
        finals.push_back({names[i], values[i]});
        values.erase(values.begin());
    }

    for (size_t i = index + 1; i < names.size(); i++) {
        finals.push_back({names[i], last.back()});
        last.pop_back();
    }

    // The rest of the elements remaining in `values` are the values needed in order to create the tuple for
    // `bar` in the example above

    std::vector<llvm::Type*> types;
    for (auto& value : values) {
        types.push_back(value->getType());
    }

    llvm::StructType* type = this->create_tuple_type(types);
    llvm::AllocaInst* alloca = this->create_alloca(type);

    for (size_t i = 0; i < values.size(); i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, alloca, i);
        this->builder->CreateStore(values[i], ptr);
    }

    func->scope->variables[consume_rest] = Variable::from_alloca(consume_rest, alloca);
    for (auto& pair : finals) {
        alloca = this->create_alloca(pair.second->getType());
        this->builder->CreateStore(pair.second, alloca);

        func->scope->variables[pair.first] = Variable::from_alloca(pair.first, alloca);
    }
}