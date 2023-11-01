#include <quart/visitor.h>

using namespace quart;

struct TupleElement {
    std::string name;
    quart::Value value;

    bool is_mutable;

    Span span;
};

llvm::Value* Visitor::make_tuple(std::vector<llvm::Value*> values, llvm::StructType* type) {
    llvm::Value* tuple = llvm::UndefValue::get(type);
    for (size_t i = 0; i < values.size(); i++) {
        tuple = this->builder->CreateInsertValue(tuple, values[i], i);
    }

    return tuple;
}

Value Visitor::visit(ast::TupleExpr* expr) {
    std::vector<quart::Type*> types;
    std::vector<llvm::Value*> values;

    for (auto& element : expr->elements) {
        Value value = element->accept(*this);
        if (value.is_empty_value()) {
            ERROR(element->span, "Expected an expression");
        }
        values.push_back(value.inner);
        types.push_back(value.type);
    }

    bool all_constant = llvm::all_of(values, [](llvm::Value* value) {
        return llvm::isa<llvm::Constant>(value);
    });

    quart::TupleType* type = this->registry->create_tuple_type(types);
    llvm::StructType* structure = llvm::cast<llvm::StructType>(type->to_llvm_type());

    if (all_constant) {
        std::vector<llvm::Constant*> constants;
        for (auto& value : values) {
            constants.push_back(llvm::cast<llvm::Constant>(value));
        }

        return Value(llvm::ConstantStruct::get(structure, constants), type, Value::Constant);
    }

    if (!this->current_function) {
        ERROR(expr->span, "Tuple literals cannot contain non-constant elements");
    }

    return Value(this->make_tuple(values, structure), type);
}  

void Visitor::store_tuple(
    const Span& span, 
    std::shared_ptr<Function> func, 
    const Value& value, 
    const std::vector<ast::Ident>& identifiers,
    std::string consume_rest
) {
    std::vector<Value> values;
    if (consume_rest.empty()) {
        values = this->unpack(value, identifiers.size(), span);

        for (auto entry : llvm::zip(identifiers, values)) {
            const ast::Ident& ident = std::get<0>(entry);
            const Value& value = std::get<1>(entry);

            llvm::AllocaInst* alloca = this->alloca(value->getType());
            this->builder->CreateStore(value, alloca);

            u8 flags = ident.is_mutable ? Variable::Mutable : Variable::None;
            func->scope->variables[ident.value] = Variable::from_alloca(
                ident.value, alloca, value.type, flags, ident.span
            );
        }

        return;
    }

    if (!consume_rest.empty() && identifiers.size() == 1) {
        llvm::AllocaInst* alloca = this->alloca(value->getType());
        this->builder->CreateStore(value, alloca);

        func->scope->variables[consume_rest] = Variable::from_alloca(
            consume_rest, alloca, value.type, Variable::Mutable, span
        );

        return;
    }

    // `let (foo, *bar, baz) = (1, 2, 3, 4, 5);`
    // here, foo takes the value of 1, baz 5 and bar is a tuple containing the rest of the elements.

    u32 n = 0;
    quart::Type* vtype = value.type;

    // TODO: Array support(?)
    if (vtype->is_pointer()) {
        quart::Type* type = vtype->get_pointee_type();
        if (!type->is_tuple()) {
            ERROR(span, "Expected a tuple type but got '{0}' instead", type->get_as_string());
        }

        n = type->get_tuple_size();
    } else {
        if (!vtype->is_tuple()) {
            ERROR(span, "Expected a tuple type but got '{0}' instead", vtype->get_as_string());
        }

        n = vtype->get_tuple_size();
    }

    values = this->unpack(value, n, span);

    // We need to know how many variables are before and after the consume rest and it's position, from there
    // we can chop off the values and store them in their respective variables and the values left will be
    // stored in the consume rest variable as a tuple.
    auto iterator = llvm::find_if(identifiers, [consume_rest](const ast::Ident& ident) {
        return ident.value == consume_rest;
    });

    size_t index = iterator - identifiers.begin();
    size_t rest = identifiers.size() - index - 1;

    // We are forced to take elements from the back because we don't know how many elements would be in the rest tuple
    // We take as many values from the back as we have variables after the consume rest (in the case above it's 1)
    std::vector<quart::Value> last;
    for (size_t i = 0; i < rest; i++) {
        last.push_back(values.back());
        values.pop_back();
    }

    std::vector<TupleElement> finals;
    for (size_t i = 0; i < index; i++) {
        const ast::Ident& ident = identifiers[i];

        finals.push_back({ident.value, values[i], ident.is_mutable, ident.span});
        values.erase(values.begin());
    }

    for (size_t i = index + 1; i < identifiers.size(); i++) {
        const ast::Ident& ident = identifiers[i];

        finals.push_back({ident.value, last.back(), ident.is_mutable, ident.span});
        last.pop_back();
    }

    // The rest of the elements remaining in `values` are the values needed in order to create the tuple for
    // `bar` in the example above
    std::vector<quart::Type*> types;
    for (auto& value : values) {
        types.push_back(value.type);
    }

    quart::TupleType* tuple = this->registry->create_tuple_type(types);
    llvm::StructType* type = llvm::cast<llvm::StructType>(tuple->to_llvm_type());

    llvm::AllocaInst* alloca = this->alloca(type);
    for (size_t i = 0; i < values.size(); i++) {
        llvm::Value* ptr = this->builder->CreateStructGEP(type, alloca, i);
        this->builder->CreateStore(values[i], ptr);
    }

    func->scope->variables[consume_rest] = Variable::from_alloca(
        consume_rest, alloca, tuple, Variable::Mutable, span
    );

    for (auto& entry : finals) {
        alloca = this->alloca(entry.value->getType());
        this->builder->CreateStore(entry.value, alloca);

        func->scope->variables[entry.name] = Variable::from_alloca(
            entry.name, alloca, entry.value.type, entry.is_mutable, entry.span
        );
    }
}