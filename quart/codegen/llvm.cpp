#include <llvm-17/llvm/IR/Constants.h>
#include <llvm-17/llvm/Pass.h>
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

    m_registers.resize(m_state.register_count());
}

void LLVMCodeGen::generate(bytecode::Move* inst) {
    llvm::Value* src = valueof(inst->src());
    this->set_register(inst->dst(), src);
}

void LLVMCodeGen::generate(bytecode::NewString* inst) {
    llvm::Value* value = m_ir_builder->CreateGlobalStringPtr(inst->value(), ".str", 0, &*m_module);
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


GENERIC_ARITH(And, LogicalAnd)
GENERIC_ARITH(Or, LogicalOr)
GENERIC_ARITH(BinaryAnd, And)
GENERIC_ARITH(BinaryOr, Or)
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

    // FIXME: This is a bit of a mess
    for (auto& parameter : function->parameters()) {
        llvm::Type* type = parameter.type->to_llvm_type(*m_context);
        llvm::AllocaInst* alloca = m_ir_builder->CreateAlloca(type, nullptr);

        llvm::Argument* arg = llvm_function->getArg(parameter.index);
        arg->setName(parameter.name);

        m_ir_builder->CreateStore(arg, alloca);
        local_scope.set_local(parameter.index, alloca);
    }

    size_t start_index = function->parameters().size();
    for (auto [index, local] : llvm::enumerate(function->locals())) {
        if (index < start_index) {
            continue;
        }

        llvm::Type* type = local->to_llvm_type(*m_context);
        llvm::AllocaInst* alloca = m_ir_builder->CreateAlloca(type, nullptr);

        local_scope.set_local(index, alloca);
    }

    m_local_scopes[function] = move(local_scope);
    m_local_scope = &m_local_scopes[function];
}

void LLVMCodeGen::generate(bytecode::GetLocal* inst) {
    llvm::AllocaInst* local = m_local_scope->local(inst->index());
    llvm::Value* value = m_ir_builder->CreateLoad(local->getAllocatedType(), local);

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::GetLocalRef* inst) {
    llvm::AllocaInst* local = m_local_scope->local(inst->index());
    this->set_register(inst->dst(), local);
}

void LLVMCodeGen::generate(bytecode::SetLocal* inst) {
    llvm::AllocaInst* local = m_local_scope->local(inst->index());

    bytecode::Operand src = inst->src();
    llvm::Value* value = nullptr;

    if (src.is_none()) {
        llvm::Type* type = local->getAllocatedType();
        value = llvm::Constant::getNullValue(type);
    } else {
        value = valueof(src);
    }

    m_ir_builder->CreateStore(value, local);
}

void LLVMCodeGen::generate(bytecode::GetGlobal*) {}
void LLVMCodeGen::generate(bytecode::GetGlobalRef*) {}
void LLVMCodeGen::generate(bytecode::SetGlobal*) {}

LLVMCodeGen::GEPResult LLVMCodeGen::create_gep(bytecode::Operand src, u32 index) {
    Vector<llvm::Value*> indices = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context),0),
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(*m_context), index)
    };

    Type* type = m_state.type(src)->get_pointee_type();
    if (type->is_array()) {
        type = type->get_array_element_type();
    }

    Type* underlying_type = nullptr;
    if (type->is_struct()) {
        underlying_type = type->get_struct_field_at(index);
    } else {
        underlying_type = type;
    }

    llvm::Value* value = m_ir_builder->CreateGEP(type->to_llvm_type(*m_context), valueof(src), indices);
    return { value, underlying_type->to_llvm_type(*m_context) };
}

void LLVMCodeGen::generate(bytecode::GetMember* inst) {
    auto [value, underlying_type] = this->create_gep(inst->src(), inst->index());
    llvm::Value* result = m_ir_builder->CreateLoad(underlying_type, value);

    this->set_register(inst->dst(), result);
}

void LLVMCodeGen::generate(bytecode::SetMember*) {}

void LLVMCodeGen::generate(bytecode::GetMemberRef* inst) {
    auto [value, _] = this->create_gep(inst->src(), inst->index());
    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Read* inst) {
    llvm::Value* src = valueof(inst->src());
    Type* type = m_state.type(inst->src());

    llvm::Type* pointee = type->get_pointee_type()->to_llvm_type(*m_context);
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

    llvm::Type* function_type = function->underlying_type()->to_llvm_type(*m_context);
    auto* llvm_function = llvm::Function::Create(
        llvm::cast<llvm::FunctionType>(function_type), llvm::Function::ExternalLinkage, function->qualified_name(), &*m_module
    );

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

    llvm::Value* value = m_ir_builder->CreateCall({ function_type, function }, arguments);
    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::generate(bytecode::Cast*) {}

void LLVMCodeGen::generate(bytecode::NewArray*) {}

void LLVMCodeGen::generate(bytecode::NewStruct* inst) {
    Struct* structure = inst->structure();
    if (structure->opaque()) {
        auto* type = llvm::StructType::create(*m_context, structure->name());
        structure->underlying_type()->set_llvm_struct_type(type);

        m_structs[structure] = type;
        return;
    }

    auto range = llvm::map_range(structure->fields(), [this](auto& entry) {
        return entry.second.type->to_llvm_type(*m_context);
    });

    Vector<llvm::Type*> fields = Vector<llvm::Type*>(range.begin(), range.end());
    auto* type = llvm::StructType::create(*m_context, fields, structure->name());

    structure->underlying_type()->set_llvm_struct_type(type);
    m_structs[structure] = type;
}

void LLVMCodeGen::generate(bytecode::Construct* inst) {
    Struct* structure = inst->structure();
    llvm::StructType* type = m_structs[structure];

    auto range = llvm::map_range(inst->arguments(), [this](auto& operand) {
        return valueof(operand);
    });

    llvm::Value* value = llvm::UndefValue::get(type);
    for (auto [index, field] : llvm::enumerate(range)) {
        value = m_ir_builder->CreateInsertValue(value, field, index);
    }

    this->set_register(inst->dst(), value);
}

void LLVMCodeGen::set_register(bytecode::Register reg, llvm::Value* value) {
    m_registers[reg.index()] = value;
}

llvm::Value* LLVMCodeGen::valueof(bytecode::Register reg) {
    return m_registers[reg.index()];
}

llvm::Value* LLVMCodeGen::valueof(bytecode::Operand operand) {
    if (operand.is_register()) {
        return m_registers[operand.value()];
    }

    llvm::Type* type = operand.value_type()->to_llvm_type(*m_context);
    return llvm::ConstantInt::get(type, operand.value());
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

        ENUMERATE_BYTECODE_INSTRUCTIONS(Op)
    #undef Op
    }
}

ErrorOr<void> LLVMCodeGen::generate(CompilerOptions const& options) {
    for (auto& inst : m_state.global_instructions()) {
        this->generate(inst);
    }

    auto& basic_blocks = m_state.generator().blocks();
    for (auto& block : basic_blocks) {
        m_ir_builder->SetInsertPoint(this->create_block_from(&*block));

        for (auto& instruction : block->instructions()) {
            this->generate(instruction);
        }
    }

    llvm::LoopAnalysisManager lam;
    llvm::FunctionAnalysisManager fam;
    llvm::CGSCCAnalysisManager cgam;
    llvm::ModuleAnalysisManager mam;

    llvm::PassBuilder builder;

    builder.registerModuleAnalyses(mam);
    builder.registerCGSCCAnalyses(cgam);
    builder.registerFunctionAnalyses(fam);
    builder.registerLoopAnalyses(lam);

    builder.crossRegisterProxies(lam, fam, cgam, mam);

    llvm::FunctionPassManager fpm = builder.buildFunctionSimplificationPipeline(
        llvm::OptimizationLevel::O1, llvm::ThinOrFullLTOPhase::None
    );

    fpm.addPass(llvm::VerifierPass());
    for (auto& function : m_module->functions()) {
        if (function.isDeclaration()) {
            continue;
        }

        fpm.run(function, fam);
    }

    llvm::verifyModule(*m_module);

    String triple, error;
    if (options.has_target()) {
        triple = options.target;
    } else {
        triple = llvm::sys::getDefaultTargetTriple();
    }

    llvm::InitializeAllTargetInfos();
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmParsers();
    llvm::InitializeAllAsmPrinters();

    auto* target = llvm::TargetRegistry::lookupTarget(triple, error);
    if (!target) {
        return err("Failed to lookup target '{0}'", triple);
    }

    llvm::TargetOptions target_options;
    auto reloc = Optional<llvm::Reloc::Model>(llvm::Reloc::Model::PIC_);

    OwnPtr<llvm::TargetMachine> machine(
        target->createTargetMachine(triple, "generic", "", target_options, reloc)
    );

    m_module->setDataLayout(machine->createDataLayout());
    m_module->setTargetTriple(triple);

    String output = options.input.with_extension("o");

    std::error_code ec;
    llvm::raw_fd_ostream stream(output, ec);

    if (ec) {
        return err("Failed to open file '{0}': {1}", output, ec.message());
    }

    llvm::legacy::PassManager pm;
    machine->addPassesToEmitFile(pm, stream, nullptr, llvm::CGFT_ObjectFile);

    pm.run(*m_module);
    stream.flush();
    
    llvm::llvm_shutdown();
    return {}; 
}

}