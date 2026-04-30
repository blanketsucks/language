#include <quart/codegen/x86_64/codegen.h>
#include <quart/language/state.h>

#include <unordered_set>

namespace quart::x86_64 {

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

Register x86_64CodeGen::generate_binary_op(BinaryInstruction instruction, bytecode::Operand lhs, bytecode::Operand rhs) {
    auto cg = m_current_function;
    Register r1 = {};

    if (lhs.is_value()) {
        r1 = this->pop_reg();
        cg->fwriteln("  mov {}, {}", r1.as_qword(), lhs.value());
    } else {
        r1 = m_register_map[lhs.reg()];
    }

    // TODO: Optimize for some instructions like `imul` where r1 could be the accumulator
    //       and in such case the generated instruction could simply be `imul r2`
    if (rhs.is_value()) {
        cg->fwriteln("  {} {}, {}", instruction, r1.as_qword(), rhs.value());
    } else {
        Register r2 = m_register_map[rhs.reg()];
        cg->fwriteln("  {} {}, {}", instruction, r1.as_qword(), r2.as_qword());

        this->push_reg(r2);
    }

    return r1;
}

Register x86_64CodeGen::generate_binary_op_with_dst(
    BinaryInstruction instruction, bytecode::Register dst, bytecode::Operand lhs, bytecode::Operand rhs
) {
    Register reg = this->generate_binary_op(instruction, lhs, rhs);
    m_register_map[dst] = reg;

    return reg;
}

void x86_64CodeGen::generate_condition(
    ConditionCode cc, bytecode::Instruction* instruction, bytecode::Register dst, bytecode::Operand lhs, bytecode::Operand rhs
) {
    auto cg = m_current_function;
    Register reg = this->generate_binary_op_with_dst(
        BinaryInstruction::cmp,
        dst, lhs, rhs
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
            stream << cg->code() << '\n';
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
    switch (inst->type()) {
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

void x86_64CodeGen::generate(bytecode::Move* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    cg->fwriteln("  mov {}, {}", dst.as_qword(), inst->src());
    
    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::GetLocal* inst) {
    auto cg = m_current_function;
    auto local = cg->local(inst->index());

    ASSERT(local.has_value(), "Local does not exist");

    Register dst = this->pop_reg();

    cg->fwriteln("  mov {}, QWORD [rbp - {}]", dst.as_qword(), local->offset);
    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::GetLocalRef* inst) {
    auto cg = m_current_function;
    auto local = cg->local(inst->index());

    ASSERT(local.has_value(), "Local does not exist");

    Register dst = this->pop_reg();

    cg->fwriteln("  lea {}, QWORD [rbp - {}]", dst.as_qword(), local->offset);
    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::SetLocal* inst) {
    auto src = inst->src();

    auto cg = m_current_function;
    auto local = cg->local(inst->index());

    ASSERT(local.has_value(), "Local does not exist");

    if (!src.has_value()) {
        cg->fwriteln("  mov QWORD [rbp - {}], 0", local->offset);
        return;
    } else if (src->is_register()) {
        Register reg = m_register_map[src->reg()];
        cg->fwriteln("  mov QWORD [rbp - {}], {}", local->offset, reg.as_qword());

        this->push_reg(reg);
    } else {
        Register reg = this->pop_reg();
        
        cg->fwriteln("  mov {}, {}", reg.as_qword(), src->value());
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
    Register src = m_register_map[inst->src()];

    bytecode::Operand index = inst->index();
    Type* type = m_state.type(inst->src())->get_pointee_type();

    if (type->is_pointer()) {
        type = type->get_pointee_type();
    } else {
        type = type->get_array_element_type();
    }

    size_t byte_size = type->size();
    auto data_type = static_cast<DataType>(byte_size);

    StringView instruction = "mov";
    if (data_type != DataType::QWord) {
        instruction = "movzx";
    }

    if (index.is_register()) {
        Register idx = m_register_map[index.reg()];
        cg->fwriteln("  {} {}, {} [{} + {} * {}]", instruction, dst.as_qword(), data_type, src.as_qword(), idx.as_qword(), byte_size);

        this->push_reg(idx);
    } else {
        cg->fwriteln("  {} {}, {} [{} + {} * {}]", instruction, dst.as_qword(), data_type, src.as_qword(), index.value(), byte_size);
    }

    this->push_reg(src);
    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::SetMember*) {
    ASSERT(false, "Not implemented");
}

void x86_64CodeGen::generate(bytecode::GetMemberRef* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = m_register_map[inst->src()];

    bytecode::Operand index = inst->index();
    Type* type = m_state.type(inst->src())->get_pointee_type();

    size_t byte_size = type->size();
    if (index.is_register()) {
        Register idx = m_register_map[index.reg()];
        cg->fwriteln("  lea {}, QWORD [{} + {} * {}]", dst.as_qword(), src.as_qword(), idx.as_qword(), byte_size);

        this->push_reg(idx);
    } else {
        cg->fwriteln("  lea {}, QWORD [{} + {} * {}]", dst.as_qword(), src.as_qword(), index.value(), byte_size);
    }

    this->push_reg(src);
    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::Alloca*) {}

void x86_64CodeGen::generate(bytecode::Read* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = m_register_map[inst->src()];

    cg->fwriteln("  mov {}, QWORD [{}]", dst.as_qword(), src.as_qword());
    this->push_reg(src);

    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::Write* inst) {
    auto cg = m_current_function;

    Register dst = this->pop_reg();
    Register src = {};

    if (inst->src().is_value()) {
        src = this->pop_reg();
        cg->fwriteln("  mov {}, {}", src.as_qword(), inst->src().value());
    } else {
        src = m_register_map[inst->src().reg()];
    }

    cg->fwriteln("  mov QWORD [{}], {}", dst.as_qword(), src.as_qword());
    this->push_reg(src);

    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::Jump* inst) {
    auto cg = m_current_function;
    cg->fwriteln("  jmp .{}", inst->target()->name());
}

void x86_64CodeGen::generate(bytecode::JumpIf* inst) {
    auto cg = m_current_function;
    auto condition = inst->condition();

    Register reg = {};
    if (condition.is_value()) {
        reg = this->pop_reg();
        cg->fwriteln("  mov {}, {}", reg.as_qword(), condition.value());
    } else {
        reg = m_register_map[condition.reg()];
    }

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

void x86_64CodeGen::generate(bytecode::GetFunction* inst) {
    auto cg = m_current_function;
    String name = normalize(inst->function()->qualified_name());

    auto& uses = m_state.register_uses(inst->dst());
    if (uses.contains<bytecode::Call>()) {
        m_next_calls.push(inst->function());
        return;
    }

    Register dst = this->pop_reg();

    cg->fwriteln("  mov {}, {}", dst.as_qword(), name);
    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::Return* inst) {
    auto cg = m_current_function;
    auto value = inst->value();

    if (!value.has_value()) {
        cg->writeln("  leave");
        cg->writeln("  ret");
    } else if (value->is_register()) {
        Register reg = m_register_map[value->reg()];
        if (reg.type == Register::rax) {
            cg->writeln("  leave");
            cg->writeln("  ret");
            
            this->push_reg(reg);
            return;
        }

        cg->fwriteln("  mov rax, {}", reg.as_qword());
        cg->writeln("  leave");
        cg->writeln("  ret");
    } else {
        cg->fwriteln("  mov rax, {}", value->value());
        cg->writeln("  leave");
        cg->writeln("  ret");
    }
}

void x86_64CodeGen::generate(bytecode::Call* inst) {
    auto cg = m_current_function;

    size_t index = 0;
    for (auto& operand : inst->arguments()) {
        Register dst { SYS_V_CALL_REGISTERS[index] };
        if (operand.is_value()) {
            cg->fwriteln("  mov {}, {}", dst.as_qword(), operand.value());
            index++;

            continue;
        }

        Register reg = m_register_map[operand.reg()];
        if (reg.type == dst.type) {
            index++;
            continue;
        }

        cg->fwriteln("  mov {}, {}", dst.as_qword(), reg.as_qword());
        index++;

        this->push_reg(reg);
    }

    if (!m_next_calls.empty()) {
        Function* function = m_next_calls.top();
        m_next_calls.pop();

        String name = normalize(function->qualified_name());
        cg->fwriteln("  call {}", name);

        if (m_next_calls.empty()) {
            this->push_reg({ Register::rax });
        }
    } else {
        Register function = m_register_map[inst->function()];
        cg->fwriteln("  call {}", function.as_qword());

        this->push_reg(function);
    }

    Register dst = this->pop_reg();

    Type* return_type = inst->function_type()->return_type();
    if (dst.type != Register::rax && !return_type->is_void()) {
        cg->fwriteln("  mov {}, rax", dst.as_qword());
    }

    m_register_map[inst->dst()] = dst;
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

void x86_64CodeGen::generate(bytecode::NewString* inst) {
    auto cg = m_current_function;
    
    size_t offset = m_strings.size();
    m_strings.push_back(inst->value());

    Register dst = this->pop_reg();
    cg->fwriteln("  mov {}, __str.{}", dst.as_qword(), offset);

    m_register_map[inst->dst()] = dst;
}

void x86_64CodeGen::generate(bytecode::Add* inst) {
    this->generate_binary_op_with_dst(
        BinaryInstruction::add,
        inst->dst(), inst->lhs(), inst->rhs()
    );
}

void x86_64CodeGen::generate(bytecode::Sub* inst) {
    this->generate_binary_op_with_dst(
        BinaryInstruction::sub,
        inst->dst(), inst->lhs(), inst->rhs()
    );
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
    this->generate_condition(ConditionCode::e, inst, inst->dst(), inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Neq* inst) {
    this->generate_condition(ConditionCode::ne, inst, inst->dst(), inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Gt* inst) {
    this->generate_condition(ConditionCode::g, inst, inst->dst(), inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Lt* inst) {
    this->generate_condition(ConditionCode::l, inst, inst->dst(), inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Gte* inst) {
    this->generate_condition(ConditionCode::ge, inst, inst->dst(), inst->lhs(), inst->rhs());
}

void x86_64CodeGen::generate(bytecode::Lte* inst) {
    this->generate_condition(ConditionCode::le, inst, inst->dst(), inst->lhs(), inst->rhs());
}
 
}