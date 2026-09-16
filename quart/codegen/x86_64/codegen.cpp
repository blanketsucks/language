#include <quart/codegen/x86_64/codegen.h>
#include <quart/language/state.h>

#include <unordered_set>

namespace quart::x86_64 {

using bytecode::Value, bytecode::Constant;

using bytecode::ConstantFloat, bytecode::ConstantInt, bytecode::ConstantString;
using bytecode::ConstantNull, bytecode::ConstantArray, bytecode::ConstantStruct;

static const Vector<Register::Type> SYS_V_CALL_REGISTERS = {
    Register::rdi, Register::rsi, Register::rdx, Register::rcx, Register::r8, Register::r9,
};

static const std::unordered_set<Register::Type> SYS_V_CALLEE_SAVED_REGISTERS = {
    Register::rbx, Register::r12, Register::r13, Register::r14, Register::r15
};

x86_64CodeGen::x86_64CodeGen(State& state, String module) : m_state(state), m_module(move(module)) {
    reset_all_registers();
}

void x86_64CodeGen::reset_all_registers() {
    m_available_registers = {};

    m_available_registers.push({ Register::r11 });
    m_available_registers.push({ Register::r10 });
    m_available_registers.push({ Register::r9 });
    m_available_registers.push({ Register::r8 });
    m_available_registers.push({ Register::rdi });
    m_available_registers.push({ Register::rsi });
    m_available_registers.push({ Register::rdx });
    m_available_registers.push({ Register::rcx });
    m_available_registers.push({ Register::rax });

    for (auto& reg : SYS_V_CALLEE_SAVED_REGISTERS) {
        m_callee_saved_registers.push({ reg });
    }
}
 
Register x86_64CodeGen::pop_reg() {
    Register reg = m_available_registers.top();
    m_available_registers.pop();

    if (reg.type == Register::rax && !m_next_calls.empty()) {
        return pop_reg();
    }

    return reg;
}

void x86_64CodeGen::push_reg(Register reg) {
    m_available_registers.push(reg);
}

void x86_64CodeGen::save(Value* value, Register dst) {
    if (!value->is_used()) {
        // If this value is never used for anything, we don't need to hold it forever.
        this->push_reg(dst);
        return;
    }

    m_register_map[value] = dst;
}

Register x86_64CodeGen::value_to_reg(Value* value, Optional<Register> dst) {
    auto cg = m_current_function;

    // FIXME: This should be isa<Constant> and every other constant type should be handled
    if (isa<ConstantInt>(value)) {
        Register reg = opt_value_or(dst, [this]() { return this->pop_reg(); });
        auto* constant = cast<ConstantInt>(value);

        cg->fwriteln("  mov {}, {}", reg.as_qword(), constant->value());
        return reg;
    } else if(isa<ConstantString>(value)) {
        auto* constant = cast<ConstantString>(value); 

        size_t offset = m_strings.size();
        m_strings.push_back(constant->value());

        Register reg = opt_value_or(dst, [this]() { return this->pop_reg(); });
        cg->fwriteln("  mov {}, __str.{}", reg.as_qword(), offset);

        return reg;
    } else {
        return m_register_map[value];
    }
}

String x86_64CodeGen::normalize(String qualified_name) {
    static constexpr StringView DOUBLE_COLON = "::";
    static constexpr StringView DOT = ".";

    auto pos = qualified_name.find(DOUBLE_COLON);
    while (pos != String::npos) {
        qualified_name.replace(pos, DOUBLE_COLON.size(), ".");
        pos = qualified_name.find(DOUBLE_COLON, pos + DOT.size());
    }

    return qualified_name;
}

Register x86_64CodeGen::generate_memory_access(Value* src, Value* index, bool ref) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src_r = m_register_map[src];

    auto type = src->type()->get_pointee_type();

    size_t byte_size = type->size();
    auto data_type = static_cast<DataType>(byte_size);

    StringView instruction = "mov";
    if (ref) {
        instruction = "lea";
    } else if (data_type != DataType::QWord) {
        instruction = "movzx";
    }

    if (isa<ConstantInt>(index)) {
        u64 value = cast<ConstantInt>(index)->value();
        cg->fwriteln(
            "  {} {}, {} [{} + {} * {}]",
            instruction, dst.as_qword(), data_type, src_r.as_qword(), value, byte_size
        );
    } else {
        Register idx = m_register_map[index];
        cg->fwriteln(
            "  {} {}, {} [{} + {} * {}]",
            instruction, dst.as_qword(), data_type, src_r.as_qword(), idx.as_qword(), byte_size
        );

        this->push_reg(idx);
    }

    this->push_reg(src_r);
    return dst;
}

Register x86_64CodeGen::generate_binary_op(BinaryInstruction instruction, Value* lhs, Value* rhs) {
    auto cg = m_current_function;
    Register r1 = this->value_to_reg(lhs);

    // TODO: Optimize for some instructions like `imul` where r1 could be the accumulator
    //       and in such case the generated instruction could simply be `imul r2`
    if (isa<ConstantInt>(rhs)) {
        u64 value = cast<ConstantInt>(rhs)->value();
        cg->fwriteln("  {} {}, {}", instruction, r1.as_qword(), value);
    } else {
        Register r2 = m_register_map[rhs];
        cg->fwriteln("  {} {}, {}", instruction, r1.as_qword(), r2.as_qword());

        this->push_reg(r2);
    }

    return r1;
}

Register x86_64CodeGen::generate_binary_op_with_dst(
    BinaryInstruction instruction, bytecode::Instruction* dst, Value* lhs, Value* rhs
) {
    Register reg = this->generate_binary_op(instruction, lhs, rhs);
    m_register_map[dst] = reg;

    return reg;
}

void x86_64CodeGen::generate_condition(
    ConditionCode cc, bytecode::Instruction* instruction, Value* lhs, Value* rhs
) {
    auto cg = m_current_function;
    Register reg = this->generate_binary_op_with_dst(
        BinaryInstruction::cmp,
        instruction, lhs, rhs
    );

    if (instruction->next()->is<bytecode::JumpIf>()) {
        m_next_cc = cc;
        return;
    }

    cg->fwriteln("  set{} {}", cc, reg.as_byte());
}

ErrorOr<void> x86_64CodeGen::generate(const CompilerOptions& options) {
    auto& functions = m_state.functions();
    
    for (auto& instruction : m_state.global_instructions()) {
        this->generate(instruction.get());
    }

    for (auto& [name, function] : functions) {
        outln("{} {}", name, function->users().size());
        if (function->should_eliminate()) {
            continue;
        }

        for (auto& block : function->basic_blocks()) {
            this->generate(block);
        }
    }

    String output = options.file.with_extension("s");

    std::ofstream stream(output, std::ios_base::out);
    
    stream << "section .text" << '\n' << '\n';
    {
        for (auto& external : m_extern_functions) {
            String name = normalize(external->qualified_name());
            stream << "extern" << ' ' << name << '\n';
        }

        stream << '\n';

        for (auto& [fn, cg] : m_functions) {
            String name = normalize(fn->qualified_name());
            stream << "global" << ' ' << name << '\n';
            stream << name << ':' << '\n';
            stream << cg->code().value() << '\n';
        }
    }

    stream << "section .data" << '\n' << '\n';
    {
        size_t index = 0;
        for (auto& str : m_strings) {
            stream << "__str." << index << ": db ";
            for (auto ch : str) {
                stream << (int)ch << ", ";
            }

            stream << 0 << '\n';
            index++;
        }
    }

    stream.flush();
    stream.close();

    return {};
}

void x86_64CodeGen::generate(bytecode::BasicBlock* block) {
    auto cg = m_current_function;
    if (cg) {
        cg->fwriteln(".{}:", block->name());
    }

    m_current_block = block;
    for (auto& instruction : block->instructions()) {
        this->generate(instruction.get());
    }

    m_current_block = nullptr;
}

void x86_64CodeGen::generate(bytecode::Instruction* inst) {
    switch (inst->kind()) {
    #define Op(x) /* NOLINT */                                           \
        case bytecode::Instruction::x:                                   \
            return this->generate(static_cast<bytecode::x*>(inst)); \

        ENUMERATE_BYTECODE_INSTRUCTIONS(Op) /* NOLINT */
    #undef Op
    }
}

void x86_64CodeGen::generate(bytecode::NewFunction* inst) {
    auto* function = inst->function();
    auto& parameters = function->parameters();

    if (function->has_trait_parameter() || function->should_eliminate()) {
        return;
    }

    if (function->is_extern() && function->is_decl()) {
        m_extern_functions.push_back(function);
        return;
    }

    auto cg = CodeGenFunction::create({});

    cg->writeln("  push rbp");
    cg->writeln("  mov rbp, rsp");
    
    size_t stack_space = function->locals().size() * 8;
    cg->fwriteln("  sub rsp, {}", stack_space);

    ASSERT(parameters.size() <= SYS_V_CALL_REGISTERS.size(), "TODO: Allow for more parameters");

    size_t offset = 8;
    for (auto& parameter : parameters) {
        Register reg { SYS_V_CALL_REGISTERS[parameter.index] };
        cg->fwriteln("  mov QWORD [rbp - {}], {}", offset, reg.as_qword());

        offset += 8;
    }

    offset = 8;
    for (auto& _ : function->locals()) {
        cg->add_local({ 8, offset });
        offset += 8;
    }

    m_functions[function] = cg;
}

void x86_64CodeGen::generate(bytecode::NewLocalScope* inst) {
    auto cg = m_functions[inst->function()];
    ASSERT(cg, "Codegen function does not exist");

    m_current_function = cg;
    reset_all_registers();
}

void x86_64CodeGen::generate(bytecode::GetLocal* inst) {
    auto cg = m_current_function;
    auto local = cg->local(inst->index());

    ASSERT(local.has_value(), "Local does not exist");

    Register dst = this->pop_reg();

    cg->fwriteln("  mov {}, QWORD [rbp - {}]", dst.as_qword(), local->offset);
    this->save(inst, dst);
}

void x86_64CodeGen::generate(bytecode::GetLocalRef* inst) {
    auto cg = m_current_function;
    auto local = cg->local(inst->index());

    ASSERT(local.has_value(), "Local does not exist");

    Register dst = this->pop_reg();

    cg->fwriteln("  lea {}, QWORD [rbp - {}]", dst.as_qword(), local->offset);
    this->save(inst, dst);
}

void x86_64CodeGen::generate(bytecode::SetLocal* inst) {
    auto src = inst->src();

    auto cg = m_current_function;
    auto local = cg->local(inst->index());

    ASSERT(local.has_value(), "Local does not exist");

    if (!src) {
        cg->fwriteln("  mov QWORD [rbp - {}], 0", local->offset);
        return;
    } else if (isa<ConstantInt>(src)) {
        u64 value = cast<ConstantInt>(src)->value();

        Register reg = this->pop_reg();
        
        cg->fwriteln("  mov {}, {}", reg.as_qword(), value);
        cg->fwriteln("  mov QWORD [rbp - {}], {}", local->offset, reg.as_qword());

        this->push_reg(reg);
    } else {
        Register reg = m_register_map[src];
        cg->fwriteln("  mov QWORD [rbp - {}], {}", local->offset, reg.as_qword());

        this->push_reg(reg);
    }
}

void x86_64CodeGen::generate(bytecode::GetGlobal*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::GetGlobalRef*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::SetGlobal*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::GetMember* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = value_to_reg(inst->src());

    Value* index = inst->index();
    Type* type = inst->src()->type()->get_pointee_type();

    if (type->is_pointer()) {
        type = type->get_pointee_type();
    } else if (type->is_array()) {
        type = type->get_array_element_type();
    }

    size_t byte_size = type->size();
    auto data_type = static_cast<DataType>(byte_size);

    StringView instruction = "mov";
    if (data_type != DataType::QWord) {
        instruction = "movzx";
    }

    if (isa<ConstantInt>(index)) {
        u64 value = cast<ConstantInt>(index)->value();
        cg->fwriteln("  {} {}, {} [{} + {} * {}]", instruction, dst.as_qword(), data_type, src.as_qword(), value, byte_size);
    } else {
        Register idx = m_register_map[index];
        cg->fwriteln("  {} {}, {} [{} + {} * {}]", instruction, dst.as_qword(), data_type, src.as_qword(), idx.as_qword(), byte_size);
    
        this->push_reg(idx);
    }

    this->push_reg(src);
    this->save(inst, dst);
}

void x86_64CodeGen::generate(bytecode::SetMember*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::GetMemberRef* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = value_to_reg(inst->src());

    Value* index = inst->index();
    Type* type = inst->src()->type()->get_pointee_type();

    size_t byte_size = type->size();
    if (isa<ConstantInt>(index)) {
        u64 value = cast<ConstantInt>(index)->value();
        cg->fwriteln("  lea {}, QWORD [{} + {} * {}]", dst.as_qword(), src.as_qword(), value, byte_size);
    } else {
        Register idx = m_register_map[index];
        cg->fwriteln("  lea {}, QWORD [{} + {} * {}]", dst.as_qword(), src.as_qword(), idx.as_qword(), byte_size);

        this->push_reg(idx);
    }

    this->push_reg(src);
    this->save(inst, dst);
}

void x86_64CodeGen::generate(bytecode::Alloca*) {}

void x86_64CodeGen::generate(bytecode::Read* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = m_register_map[inst->src()];

    cg->fwriteln("  mov {}, QWORD [{}]", dst.as_qword(), src.as_qword());
    this->push_reg(src);

    this->save(inst, dst);
}

void x86_64CodeGen::generate(bytecode::Write* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = this->value_to_reg(inst->src());

    cg->fwriteln("  mov QWORD [{}], {}", dst.as_qword(), src.as_qword());
    this->push_reg(src);

    this->save(inst, dst);
}

void x86_64CodeGen::generate(bytecode::Jump* inst) {
    auto cg = m_current_function;
    cg->fwriteln("  jmp .{}", inst->target()->name());
}

void x86_64CodeGen::generate(bytecode::JumpIf* inst) {
    auto cg = m_current_function;
    auto condition = inst->condition();

    Register reg = this->value_to_reg(condition);

    auto* block = m_current_block;

    auto* false_target = inst->false_target();
    auto* true_target = inst->true_target();

    ConditionCode cc = ConditionCode::z;
    if (m_next_cc != ConditionCode::None) {
        cc = m_next_cc;
        m_next_cc = ConditionCode::None;
    } else {
        cg->fwriteln("  test {0}, {0}", reg.as_qword());
    }

    if (block->next() == true_target) {
        cc = negate(cc);
        cg->fwriteln("  j{} .{}", cc, false_target->name());
    } else if (block->next() == false_target) {
        cg->fwriteln("  j{} .{}", cc, true_target->name());
    } else {
        cg->fwriteln("  j{} .{}", cc, false_target->name());
        cg->fwriteln("  jmp .{}", true_target->name());
    }

    this->push_reg(reg);
}

void x86_64CodeGen::generate(bytecode::Return* inst) {
    auto cg = m_current_function;
    auto value = inst->value();

    if (!value) {
        // fallthrough
    } else if (isa<ConstantInt>(value)) {
        auto* constant = cast<ConstantInt>(value);
        cg->fwriteln("  mov rax, {}", constant->value());
    } else {
        Register reg = m_register_map[value];
        if (reg.type != Register::rax) {
            cg->fwriteln("  mov rax, {}", reg.as_qword());
        }
        
        this->push_reg(reg);
    }

    cg->writeln("  leave");
    cg->writeln("  ret");
}

void x86_64CodeGen::generate(bytecode::Call* inst) {
    auto cg = m_current_function;

    size_t index = 0;
    for (auto& operand : inst->arguments()) {
        Register dst { SYS_V_CALL_REGISTERS[index] };
        if (isa<Constant>(operand)) {
            this->value_to_reg(operand, dst);
            index++;

            continue;
        }

        Register reg = m_register_map[operand];
        if (reg.type == dst.type) {
            index++;
            continue;
        }

        cg->fwriteln("  mov {}, {}", dst.as_qword(), reg.as_qword());
        index++;

        this->push_reg(reg);
    }

    auto* value = inst->function();
    if (isa<Function>(value)) {
        auto* function = cast<Function>(value);
        String name = normalize(function->qualified_name());

        cg->fwriteln("  call {}", name);
    } else {
        Register function = m_register_map[value];
        cg->fwriteln("  call {}", function.as_qword());

        this->push_reg(function);
    }

    Register dst = this->pop_reg();

    Type* return_type = inst->function_type()->return_type();
    if (dst.type != Register::rax && !return_type->is_void()) {
        cg->fwriteln("  mov {}, rax", dst.as_qword());
    }

    m_register_map[inst] = dst;
}

void x86_64CodeGen::generate(bytecode::Cast*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::NewArray*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::NewStruct*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Construct*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::NewTuple*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Null*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Not*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Boolean*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Memcpy*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::GetReturn*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Add* inst) {
    this->generate_binary_op_with_dst(BinaryInstruction::add, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Sub* inst) {
    this->generate_binary_op_with_dst(BinaryInstruction::sub, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Mul*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Div*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Mod*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Or*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::And*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::LogicalOr*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::LogicalAnd*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Xor*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Rsh*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Lsh*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::Eq* inst) {
    this->generate_condition(ConditionCode::e, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Neq* inst) {
    this->generate_condition(ConditionCode::ne, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Gt* inst) {
    this->generate_condition(ConditionCode::g, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Lt* inst) {
    this->generate_condition(ConditionCode::l, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Gte* inst) {
    this->generate_condition(ConditionCode::ge, inst, inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Lte* inst) {
    this->generate_condition(ConditionCode::le, inst, inst->lhs(), inst->rhs());
}
 
}