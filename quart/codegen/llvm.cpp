#include <llvm-20/llvm/IR/Constants.h>
#include <quart/codegen/llvm.h>
#include <quart/compiler.h>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/PassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/TargetParser/Host.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/TargetSelect.h>

namespace quart::codegen {

LLVMCodeGen::LLVMCodeGen(State& state, String module_name) : m_state(state) {
    m_context = make<llvm::LLVMContext>();
    m_module = make<llvm::Module>(move(module_name), *m_context);
    m_ir_builder = make<llvm::IRBuilder<>>(*m_context);

    m_registers.resize(state.register_count());
    m_globals.resize(state.global_count());
}

void LLVMCodeGen::generate(bytecode::Move* inst) {
    Type* type = m_state.type(inst->dst());

    llvm::Value* src = nullptr;
    if (type->is_float()) {
        u64 v = inst->src();
        f64 value = *(f64*)&v;

        src = llvm::ConstantFP::get(type_of(type), value);
    } else {
        src = m_ir_builder->getInt({ type->get_int_bit_width(), inst->src(), !type->is_int_unsigned() }); 
    }

    this->set_register(inst->dst(), src);
}

void LLVMCodeGen::generate(bytecode::NewString* inst) {
    llvm::Value* value = m_ir_builder->CreateGlobalString(inst->value(), ".str", 0, &*m_module);
    this->set_register(inst->dst(), value);
}

#define GENERIC_ARITH(Name, Function) /* NOLINT */                      \
    void LLVMCodeGen::generate(bytecode::Name* inst) {                  \
        llvm::Value* lhs = valueof(inst->lhs());                        \
        llvm::Value* rhs = valueof(inst->rhs());                        \
                                                                        \
        llvm::Value* value = m_ir_builder->Create##Function(lhs, rhs);  \
        this->set_register(inst->dst(), value);                         \
    }

#define GENERIC_FLOAT_ARITH(Name, FFunction, IFunction) /* NOLINT */    \
    void LLVMCodeGen::generate(bytecode::Name* inst) {                  \
        llvm::Value* lhs = valueof(inst->lhs());                        \
        llvm::Value* rhs = valueof(inst->rhs());                        \
                                                                        \
        llvm::Value* value = nullptr;                                   \
        if (lhs->getType()->isFloatingPointTy()) {                      \
            value = m_ir_builder->Create##FFunction(lhs, rhs);          \
        } else {                                                        \
            value = m_ir_builder->Create##IFunction(lhs, rhs);          \
        }                                                               \
                                                                        \
        this->set_register(inst->dst(), value);                         \
    }

#define SIGNED_ARITH(Name, FFunction, UFunction, SFunction)/* NOLINT */ \
    void LLVMCodeGen::generate(bytecode::Name* inst) {                  \
        llvm::Value* lhs = valueof(inst->lhs());                        \
        llvm::Value* rhs = valueof(inst->rhs());                        \
                                                                        \
        llvm::Value* value = nullptr;                                   \
        quart::Type* type = m_state.type(inst->lhs());                  \
                                                                        \
        if (type->is_float()) {                                         \
            value = m_ir_builder->Create##FFunction(lhs, rhs);          \
        } else if (type->is_int_unsigned()) {                           \
            value = m_ir_builder->Create##UFunction(lhs, rhs);          \
        } else {                                                        \
            value = m_ir_builder->Create##SFunction(lhs, rhs);          \
        }                                                               \
                                                                        \
        this->set_register(inst->dst(), value);                         \
    }


GENERIC_ARITH(And, And)
GENERIC_ARITH(Or, Or)
GENERIC_ARITH(LogicalAnd, LogicalAnd)
GENERIC_ARITH(LogicalOr, LogicalOr)
GENERIC_ARITH(Xor, Xor)
GENERIC_ARITH(Lsh, Shl)
GENERIC_ARITH(Rsh, LShr)

GENERIC_FLOAT_ARITH(Add, FAdd, Add)
GENERIC_FLOAT_ARITH(Sub, FSub, Sub)
GENERIC_FLOAT_ARITH(Mul, FMul, Mul)
SIGNED_ARITH(Div, FDiv, UDiv, SDiv)
SIGNED_ARITH(Mod, FRem, URem, SRem)

GENERIC_FLOAT_ARITH(Eq, FCmpOEQ, ICmpEQ)
GENERIC_FLOAT_ARITH(Neq, FCmpONE, ICmpNE)
SIGNED_ARITH(Gt, FCmpUGT, ICmpUGT, ICmpSGT)
SIGNED_ARITH(Gte, FCmpUGE, ICmpUGE, ICmpSGE)
SIGNED_ARITH(Lt, FCmpULT, ICmpULT, ICmpSLT)
SIGNED_ARITH(Lte, FCmpULE, ICmpULE, ICmpSLE)

void LLVMCodeGen::generate(bytecode::NewLocalScope* inst) {
    auto* function = inst->function();
    LocalScope local_scope(function, function->local_count());

    llvm::Function* llvm_function = m_functions[function];

    m_ir_builder->SetInsertPoint(m_basic_blocks[function->entry_block()]);
    if (function->is_struct_return()) {
        llvm::Type* type = type_of(function->return_type());

        llvm::Argument* arg = llvm_function->getArg(function->is_member_method());
        llvm::Attribute attribute = llvm::Attribute::get(*m_context, llvm::Attribute::StructRet, type);

        arg->addAttr(attribute);
        local_scope.set_return(arg);
    }

    // FIXME: This is a bit of a mess
    for (auto& parameter : function->parameters()) {
        u32 index = parameter.index;
        if (function->is_struct_return()) {
            if (!parameter.is_self()) {
                index++;
            }
        }

        llvm::Type* type = type_of(parameter.type);
        llvm::Argument* arg = llvm_function->getArg(index);

        if (parameter.is_byval()) {
            llvm::Attribute attribute = llvm::Attribute::get(
                *m_context,
                llvm::Attribute::ByVal,
                type
            );

            arg->addAttr(attribute);

            local_scope.set_local(parameter.index, arg, llvm::PointerType::get(type, 0));
            continue;
        }

        llvm::AllocaInst* alloca = m_ir_builder->CreateAlloca(type, nullptr);
        m_ir_builder->CreateStore(arg, alloca);

        local_scope.set_local(parameter.index, alloca, type);
    }

    size_t start_index = function->parameters().size();
    for (auto [index, local] : llvm::enumerate(function->locals())) {
        if (index < start_index) {
            continue;
        } else if (function->is_struct_local(index)) {
            llvm::Type* type = type_of(local);
            local_scope.set_local(index, nullptr, type);

            continue;
        }
        
        llvm::Type* type = type_of(local);
        llvm::AllocaInst* alloca = m_ir_builder->CreateAlloca(type, nullptr);

        local_scope.set_local(index, alloca, type);
    }

    m_local_scopes[function] = move(local_scope);
    if (inst->set()) {
        m_local_scope = &m_local_scopes[function];
    }
}

void LLVMCodeGen::generate(bytecode::GetLocal* inst) {
    ASSERT(inst->index() < m_local_scope->local_count(), "Local index out of bounds");

    auto const& local = m_local_scope->local(inst->index());
    llvm::Value* value = m_ir_builder->CreateLoad(local.type, local.store);

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::GetLocalRef* inst) {
    ASSERT(inst->index() < m_local_scope->local_count(), "Local index out of bounds");

    auto const& local = m_local_scope->local(inst->index());
    this->set_register(inst->dst(), local.store);
}

void LLVMCodeGen::generate(bytecode::SetLocal* inst) {
    ASSERT(inst->index() < m_local_scope->local_count(), "Local index out of bounds");

    auto& local = m_local_scope->local(inst->index());
    Optional<bytecode::Operand> src = inst->src();

    if (local.needs_store()) {
        local.store = valueof(*src);
        return;
    }

    llvm::Value* value = nullptr;
    if (!src.has_value()) {
        value = llvm::Constant::getNullValue(local.type);
    } else {
        value = valueof(*src);
    }

    m_ir_builder->CreateStore(value, local.store);
}

void LLVMCodeGen::generate(bytecode::GetGlobal* inst) {
    ASSERT(inst->index() < m_globals.size(), "Global index out of bounds");

    auto* global = m_globals[inst->index()];
    this->set_register(inst->dst(), global->getInitializer());
}

void LLVMCodeGen::generate(bytecode::GetGlobalRef* inst) {
    ASSERT(inst->index() < m_globals.size(), "Global index out of bounds");

    auto* global = m_globals[inst->index()];
    this->set_register(inst->dst(), global);
}

void LLVMCodeGen::generate(bytecode::SetGlobal* inst) {
    ASSERT(inst->index() < m_globals.size(), "Global index out of bounds");

    auto* global = m_globals[inst->index()];
    String name = format("global.{}", inst->index());

    llvm::Type* type = type_of(inst->src()->type());
    if (!global) {
        m_module->getOrInsertGlobal(name, type);
        global = m_module->getGlobalVariable(name);

        m_globals[inst->index()] = global;
    }

    global->setInitializer(llvm::cast<llvm::Constant>(valueof(inst->src())));
}

llvm::Value* LLVMCodeGen::create_gep(bytecode::Register src, bytecode::Operand index) {
    Vector<llvm::Value*> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context),0),
        valueof(index)
    };

    Type* type = m_state.type(src)->get_pointee_type();
    if (type->is_pointer()) {
        indices = { valueof(index) };
        type = type->get_pointee_type();
    }

    return m_ir_builder->CreateGEP(type_of(type), valueof(src), indices);
}

void LLVMCodeGen::generate(bytecode::GetMember* inst) {
    llvm::Value* value = this->create_gep(inst->src(), inst->index());
    llvm::Type* underlying_type = type_of(m_state.type(inst->dst()));

    llvm::Value* result = m_ir_builder->CreateLoad(underlying_type, value);
    this->set_register(inst->dst(), result);
}

void LLVMCodeGen::generate(bytecode::SetMember* inst) {
    llvm::Value* value = this->create_gep(inst->dst(), inst->index());
    m_ir_builder->CreateStore(valueof(inst->src()), value);
}

void LLVMCodeGen::generate(bytecode::GetMemberRef* inst) {
    llvm::Value* value = this->create_gep(inst->src(), inst->index());
    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Alloca* inst) {
    llvm::Type* type = type_of(inst->type());

    llvm::BasicBlock* block = m_ir_builder->GetInsertBlock();
    llvm::Function* function = block->getParent();

    llvm::IRBuilder<> tmp(&function->getEntryBlock(), function->getEntryBlock().begin());
    llvm::Value* alloca = tmp.CreateAlloca(type, nullptr);

    this->set_register(inst->dst(), alloca);
}

void LLVMCodeGen::generate(bytecode::Read* inst) {
    llvm::Value* src = valueof(inst->src());
    Type* type = m_state.type(inst->src());

    llvm::Type* pointee = type_of(type->underlying_type());
    llvm::Value* value = m_ir_builder->CreateLoad(pointee, src);

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Write* inst) {
    llvm::Value* src = valueof(inst->src());
    llvm::Value* dst = valueof(inst->dst());

    m_ir_builder->CreateStore(src, dst);
}

void LLVMCodeGen::generate(bytecode::Jump* inst) {
    llvm::BasicBlock* block = m_basic_blocks[inst->target()];
    m_ir_builder->CreateBr(block);
}

void LLVMCodeGen::generate(bytecode::JumpIf* inst) {
    llvm::Value* condition = valueof(inst->condition());

    llvm::BasicBlock* true_block = m_basic_blocks[inst->true_target()];
    llvm::BasicBlock* false_block = m_basic_blocks[inst->false_target()];

    m_ir_builder->CreateCondBr(condition, true_block, false_block);
}

void LLVMCodeGen::generate(bytecode::NewFunction* inst) {
    auto* function = inst->function();
    if (function->has_trait_parameter() || function->should_eliminate()) {
        return;
    }

    auto range = llvm::map_range(
        function->parameters(),
        [](auto& parameter) -> Type* {
            if (parameter.is_byval()) {
                return parameter.type->get_pointer_to();
            }

            return parameter.type; 
        }
    );

    Vector<Type*> parameters(range.begin(), range.end());
    Type* return_type = function->return_type();

    if (function->is_struct_return()) {
        // We put the struct return as the first parameter or the second one if we have `self`
        parameters.insert(parameters.begin() + function->is_member_method(), function->return_type()->get_pointer_to());
        return_type = m_state.context().void_type();
    }

    auto r = llvm::map_range(parameters, [this](auto& type) { return type_of(type); });
    llvm::FunctionType* function_type = llvm::FunctionType::get(
        type_of(return_type),
        Vector<llvm::Type*>(r.begin(), r.end()),
        function->underlying_type()->is_function_var_arg()
    );

    auto* llvm_function = llvm::Function::Create(
        function_type,
        llvm::Function::ExternalLinkage,
        normalize(function->qualified_name()),
        &*m_module
    );
    
    llvm_function->setDSOLocal(!function->is_decl());
    if (!function->is_extern() && !function->is_main()) {
        llvm_function->setLinkage(llvm::GlobalValue::LinkageTypes::InternalLinkage);
    }

    m_functions[function] = llvm_function;
    for (auto& basic_block : function->basic_blocks()) {
        auto* block = this->create_block_from(basic_block);
        block->insertInto(llvm_function);
    }
}

void LLVMCodeGen::generate(bytecode::GetFunction* inst) {
    llvm::Function* function = m_functions[inst->function()];
    this->set_register(inst->dst(), function);
}

void LLVMCodeGen::generate(bytecode::Return* inst) {
    Optional<bytecode::Operand> value = inst->value();
    if (value.has_value()) {
        m_ir_builder->CreateRet(valueof(*value));
    } else {
        m_ir_builder->CreateRetVoid();
    }
}

void LLVMCodeGen::generate(bytecode::Call* inst) {
    auto range = llvm::map_range(inst->arguments(), [&](auto& operand) { return valueof(operand); });
    Vector<llvm::Value*> arguments(range.begin(), range.end());

    auto* function_type = llvm::cast<llvm::FunctionType>(inst->function_type()->to_llvm_type(*m_context));
    llvm::Value* function = valueof(inst->function());

    if (llvm::isa<llvm::Function>(function)) {
        auto* fn = llvm::cast<llvm::Function>(function);
        llvm::Value* value = m_ir_builder->CreateCall(fn, arguments);

        this->set_register(inst->dst(), value);
        return;
    }

    llvm::Value* value = m_ir_builder->CreateCall({ function_type, function }, arguments);
    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Cast* inst) {
    llvm::Value* src = valueof(inst->src());

    Type* from = m_state.type(inst->src());
    Type* to = inst->type();

    llvm::Type* type = type_of(to);

    llvm::Value* value = src;
    if (from->is_int()) {
        if (to->is_floating_point()) {
            if (from->is_int_unsigned()) {
                value = m_ir_builder->CreateUIToFP(src, type);
            } else {
                value = m_ir_builder->CreateSIToFP(src, type);
            }
        } else if (to->is_int()) {
            if (from->is_int_unsigned()) {
                value = m_ir_builder->CreateZExtOrTrunc(src, type);
            } else {
                value = m_ir_builder->CreateSExtOrTrunc(src, type);
            }
        } else if (to->is_pointer()) {
            value = m_ir_builder->CreateIntToPtr(src, type);
        }
    } else if (from->is_floating_point()) {
        if (to->is_floating_point()) {
            value = m_ir_builder->CreateFPCast(src, type);
        } else if (to->is_int()) {
            if (to->is_int_unsigned()) {
                value = m_ir_builder->CreateFPToUI(value, type);
            } else {
                value = m_ir_builder->CreateFPToSI(value, type);
            }
        }
    } else if (from->is_pointer()) {
        if (to->is_int()) {
            if (to->get_int_bit_width() == 1) {
                value = m_ir_builder->CreateIsNotNull(src);
            } else {
                value = m_ir_builder->CreatePtrToInt(src, type);
            }
        } else if (to->is_pointer()) {
            value = m_ir_builder->CreateBitCast(src, type);
        }
    } else if (from->is_reference() && to->is_pointer()) {
        value = m_ir_builder->CreateBitCast(src, type);
    }

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::NewArray* inst) {
    auto range = llvm::map_range(inst->elements(), [this](auto& operand) { return valueof(operand); });
    llvm::Value* value = llvm::UndefValue::get(type_of(inst->type()));

    for (auto [index, field] : llvm::enumerate(range)) {
        value = m_ir_builder->CreateInsertValue(value, field, index);
    }

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::NewStruct* inst) {
    Struct* structure = inst->structure();
    if (structure->opaque()) {
        auto* type = llvm::StructType::create(*m_context, normalize(structure->qualified_name()));
        m_structs[structure] = type;

        return;
    }

    auto* type = llvm::StructType::create(*m_context, {}, normalize(structure->qualified_name()));
    auto range = llvm::map_range(structure->underlying_type()->fields(), [this](auto& entry) {
        return type_of(entry);
    });

    Vector<llvm::Type*> fields = Vector<llvm::Type*>(range.begin(), range.end());
    type->setBody(fields);

    m_structs[structure] = type;
}

void LLVMCodeGen::generate(bytecode::Construct* inst) {
    Struct* structure = inst->structure();
    llvm::StructType* type = m_structs[structure];

    if (inst->arguments().empty()) {
        llvm::Value* value = llvm::ConstantAggregateZero::get(type);
        this->set_register(inst->dst(), value);

        return;
    }

    auto range = llvm::map_range(inst->arguments(), [this](auto& operand) {
        return valueof(operand);
    });

    llvm::Value* value = llvm::UndefValue::get(type);
    for (auto [index, field] : llvm::enumerate(range)) {
        value = m_ir_builder->CreateInsertValue(value, field, index);
    }

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::NewTuple* inst) {
    TupleType* type = inst->type();
    auto iterator = m_tuple_types.find(type);

    llvm::StructType* structure = nullptr;
    if (iterator == m_tuple_types.end()) {
        auto range = llvm::map_range(type->types(), [this](auto* type) { return type_of(type); });
        Vector<llvm::Type*> types(range.begin(), range.end());

        String name = format("__tuple.{}", m_tuple_count++);
        structure = llvm::StructType::create(*m_context, types, name);
    } else {
        structure = iterator->second;
    }

    llvm::Value* value = llvm::UndefValue::get(structure);
    for (auto [index, operand] : llvm::enumerate(inst->elements())) {
        value = m_ir_builder->CreateInsertValue(value, valueof(operand), index);
    }

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Null* inst) {
    llvm::Type* type = type_of(inst->type());
    llvm::Value* value = llvm::Constant::getNullValue(type);

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Not* inst) {
    llvm::Value* value = valueof(inst->src());
    llvm::Value* result = m_ir_builder->CreateIsNull(value);

    this->set_register(inst->dst(), result);
}

void LLVMCodeGen::generate(bytecode::Boolean* inst) {
    this->set_register(inst->dst(), m_ir_builder->getInt1(inst->value()));
}

void LLVMCodeGen::generate(bytecode::Memcpy* inst) {
    llvm::Value* src = valueof(inst->src());
    llvm::Value* dst = valueof(inst->dst());

    m_ir_builder->CreateMemCpy(dst, {}, src, {}, inst->size(), false);
}

void LLVMCodeGen::generate(bytecode::GetReturn* inst) {
    llvm::Value* value = m_local_scope->return_value();
    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::set_register(bytecode::Register reg, llvm::Value* value) {
    m_registers[reg.index()] = value;
}

llvm::Value* LLVMCodeGen::valueof(bytecode::Register reg) {
    return m_registers[reg.index()];
}

llvm::Value* LLVMCodeGen::valueof(bytecode::Operand const& operand) {
    if (operand.is_register()) {
        return m_registers[operand.value()];
    }

    llvm::Type* type = type_of(operand.value_type());
    return llvm::ConstantInt::get(type, operand.value());
}

llvm::Value* LLVMCodeGen::valueof(Constant* constant) {
    switch (constant->kind()) {
        case Constant::Kind::Int: {
            auto* integer = constant->as<ConstantInt>();
            return m_ir_builder->getIntN(integer->type()->get_int_bit_width(), integer->value());
        }
        case Constant::Kind::Float: {
            auto* fp = constant->as<ConstantFloat>();
            if (fp->type()->is_float()) {
                return llvm::ConstantFP::get(*m_context, llvm::APFloat(static_cast<f32>(fp->value())));
            } else {
                return llvm::ConstantFP::get(*m_context, llvm::APFloat(fp->value()));
            }
        }
        case Constant::Kind::String: {
            auto* string = constant->as<ConstantString>();
            return m_ir_builder->CreateGlobalString(string->value(), ".str", 0, &*m_module);
        }
        case Constant::Kind::Array: {
            auto* array = constant->as<ConstantArray>();
            auto range = llvm::map_range(array->elements(), [this](auto& element) {
                return llvm::cast<llvm::Constant>(valueof(element));
            });

            Vector<llvm::Constant*> elements(range.begin(), range.end());
            llvm::Type* type = array->type()->to_llvm_type(*m_context);

            return llvm::ConstantArray::get(llvm::cast<llvm::ArrayType>(type), elements);
        }
        case Constant::Kind::Struct: {
            auto* structure = constant->as<ConstantStruct>();
            auto range = llvm::map_range(structure->fields(), [this](auto& field) {
                return llvm::cast<llvm::Constant>(valueof(field));
            });

            Vector<llvm::Constant*> elements(range.begin(), range.end());

            auto* decl = structure->type()->as<StructType>()->decl();
            llvm::Type* type = m_structs[decl];

            return llvm::ConstantStruct::get(llvm::cast<llvm::StructType>(type), elements);
        }
        case Constant::Kind::Null: {
            auto* null = constant->as<ConstantNull>();
            llvm::Type* type = type_of(null->type());

            return llvm::Constant::getNullValue(type);
        }
    }

    return nullptr;
}

llvm::Type* LLVMCodeGen::type_of(Type* type) {
    if (type->is_struct()) {
        auto* decl = type->as<StructType>()->decl();
        return m_structs[decl];
    }

    return type->to_llvm_type(*m_context);
}

String LLVMCodeGen::normalize(String qualified_name) {
    static constexpr StringView DOUBLE_COLON = "::";
    static constexpr StringView DOT = ".";

    auto pos = qualified_name.find(DOUBLE_COLON);
    while (pos != String::npos) {
        qualified_name.replace(pos, DOUBLE_COLON.size(), ".");
        pos = qualified_name.find(DOUBLE_COLON, pos + DOT.size());
    }

    return qualified_name;
}

llvm::BasicBlock* LLVMCodeGen::create_block_from(bytecode::BasicBlock* block) {
    if (m_basic_blocks.contains(block)) {
        return m_basic_blocks[block];
    }

    llvm::BasicBlock* llvm_block = llvm::BasicBlock::Create(*m_context, block->name());
    m_basic_blocks[block] = llvm_block;

    return llvm_block;
}

void LLVMCodeGen::generate(bytecode::Instruction* inst) {
    switch (inst->type()) {
    #define Op(x) /* NOLINT */                                      \
        case bytecode::Instruction::x:                              \
            return this->generate(static_cast<bytecode::x*>(inst)); \

        ENUMERATE_BYTECODE_INSTRUCTIONS(Op) /* NOLINT */
    #undef Op
    }
}

void LLVMCodeGen::generate(bytecode::BasicBlock* block) {
    m_ir_builder->SetInsertPoint(this->create_block_from(&*block));
    for (auto& instruction : block->instructions()) {
        this->generate(instruction.get());
    }
}

ErrorOr<void> LLVMCodeGen::generate(CompilerOptions const& options) {
    for (auto& global : m_state.globals()) {
        llvm::Type* type = type_of(global->value_type());
        String name = format("global.{}", global->index());

        m_module->getOrInsertGlobal(name, type);
        llvm::GlobalVariable* var = m_module->getGlobalVariable(name);

        var->setInitializer(llvm::cast<llvm::Constant>(valueof(global->initializer())));
        m_globals[global->index()] = var;
    }

    for (auto& instruction : m_state.global_instructions()) {
        this->generate(instruction.get());
    }

    auto& functions = m_state.functions();
    for (auto& [name, function] : functions) {
        if (function->should_eliminate()) {
            continue;
        }

        for (auto& block : function->basic_blocks()) {
            this->generate(block);
        }
    }

    String triple, error;
    if (options.has_target()) {
        triple = options.target;
    } else {
        triple = llvm::sys::getDefaultTargetTriple();
    }

    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) {
        return err("Failed to lookup target '{}'", triple);
    }

    llvm::TargetOptions target_options;
    auto reloc = Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    OwnPtr<llvm::TargetMachine> machine(
        target->createTargetMachine(triple, "generic", "", target_options, reloc)
    );

    m_module->setDataLayout(machine->createDataLayout());
    m_module->setTargetTriple(machine->getTargetTriple().str());

    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;

    llvm::PassBuilder builder;
    machine->registerPassBuilderCallbacks(builder);

    builder.registerModuleAnalyses(mam);
    builder.registerCGSCCAnalyses(cgam);
    builder.registerFunctionAnalyses(fam);
    builder.registerLoopAnalyses(lam);

    builder.crossRegisterProxies(lam, fam, cgam, mam);

    llvm::OptimizationLevel level;
    switch (options.opts.level) {
        case OptimizationLevel::O0:
            level = llvm::OptimizationLevel::O0; break;
        case OptimizationLevel::O1:
            level = llvm::OptimizationLevel::O1; break;
        case OptimizationLevel::O2:
            level = llvm::OptimizationLevel::O2; break;
        case OptimizationLevel::O3:
            level = llvm::OptimizationLevel::O3; break;
        case OptimizationLevel::Os:
            level = llvm::OptimizationLevel::Os; break;
        case OptimizationLevel::Oz:
            level = llvm::OptimizationLevel::Oz; break;
    }

    llvm::FunctionPassManager fpm = builder.buildFunctionSimplificationPipeline(level, llvm::ThinOrFullLTOPhase::None);
    fpm.addPass(llvm::VerifierPass());

    Vector<llvm::Function*> functions_to_erase;
    Vector<llvm::GlobalVariable*> globals_to_erase;

    for (auto& function : m_module->functions()) {
        if (function.isDeclaration()) {
            continue;
        }

        if (options.opts.level == OptimizationLevel::O0) {
            continue;
        }

        fpm.run(function, fam);
    }

    // We have to do a second run since LLVM likes to optimize some functions in
    // for example: if it seems fit, it can replace a printf call to a puts call
    for (auto& function : m_module->functions()) {
        if (function.use_empty() && function.getName() != options.entry) {
            functions_to_erase.push_back(&function);
        }
    }

    llvm::verifyModule(*m_module, &llvm::errs());
    for (auto& function : functions_to_erase) {
        function->eraseFromParent();
    }

    for (auto& global : m_module->globals()) {
        if (global.getName().starts_with("llvm.")) {
            continue;
        }

        if (global.use_empty()) {
            globals_to_erase.push_back(&global);
        }
    }

    for (auto& global : globals_to_erase) {
        m_module->eraseGlobalVariable(global);
    }
    
    {
        String out = options.file.with_extension("ll");
        std::error_code ec;
    
        llvm::raw_fd_ostream stream(out, ec);
        m_module->print(stream, nullptr);
    }

    String output = options.file.with_extension("o");

    std::error_code ec;
    llvm::raw_fd_ostream stream(output, ec);

    if (ec) {
        return err("Failed to open file '{}': {}", output, ec.message());
    }

    llvm::legacy::PassManager pm;
    machine->addPassesToEmitFile(pm, stream, nullptr, llvm::CodeGenFileType::ObjectFile);

    pm.run(*m_module);
    stream.flush();

    return {};
}

}