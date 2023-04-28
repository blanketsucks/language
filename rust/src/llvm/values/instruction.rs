use crate::llvm;

use llvm::ffi;

pub type InstructionOpcode = ffi::LLVMOpcode;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct Instruction {
    instruction: *const ffi::LLVMValue
}

impl Instruction {
    pub(crate) fn new(instruction: *const ffi::LLVMValue) -> Self {
        Self { instruction }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.instruction }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.instruction) };
        println!()
    }

    pub fn is_constant(&self) -> bool {
        unsafe { ffi::LLVMIsConstant(self.instruction) }
    }

    pub fn get_type(&self) -> llvm::Type {
        unsafe { llvm::Type::new(ffi::LLVMTypeOf(self.instruction)) }
    }

    pub fn operands(&self) -> u32 {
        unsafe { ffi::LLVMGetNumOperands(self.instruction) }
    }

    pub fn get_operand(&self, index: u32) -> llvm::Value {
        llvm::Value::new(unsafe { ffi::LLVMGetOperand(self.instruction, index) })
    }

    pub fn set_operand(&self, index: u32, value: llvm::Value) {
        unsafe { ffi::LLVMSetOperand(self.instruction, index, value.as_ptr()) }
    }

    pub fn get_name(&self) -> &'static str {
        let mut len = 0;
        unsafe { ffi::to_str(ffi::LLVMGetValueName2(self.instruction, &mut len)) }
    }

    pub fn set_name(&mut self, name: &str) {
        let len = name.len();
        let cstr = std::ffi::CString::new(name).unwrap();

        unsafe { ffi::LLVMSetValueName2(self.instruction, cstr.as_ptr(), len) }
    }

    pub fn get_parent(&self) -> Option<llvm::BasicBlock> {
        let parent = unsafe { ffi::LLVMGetInstructionParent(self.instruction) };
        if parent.is_null() {
            None
        } else {
            Some(llvm::BasicBlock::from_ptr(parent))
        }
    }

    pub fn remove_from_parent(&self) {
        unsafe { ffi::LLVMInstructionRemoveFromParent(self.instruction) }
    }

    pub fn erase_from_parent(&self) {
        unsafe { ffi::LLVMInstructionEraseFromParent(self.instruction) }
    }

    pub fn get_opcode(&self) -> InstructionOpcode {
        unsafe { ffi::LLVMGetInstructionOpcode(self.instruction) }
    }

    pub fn get_opcode_name(&self) -> &'static str {
        let opcode = self.get_opcode();
        unsafe { ffi::to_str(ffi::LLVMGetInstructionOpcodeName(opcode)) }
    }

    pub fn get_next_instruction(&self) -> Option<Self> {
        let next = unsafe { ffi::LLVMGetNextInstruction(self.instruction) };
        if next.is_null() {
            None
        } else {
            Some(Self::new(next))
        }
    }

    pub fn get_previous_instruction(&self) -> Option<Self> {
        let prev = unsafe { ffi::LLVMGetPreviousInstruction(self.instruction) };
        if prev.is_null() {
            None
        } else {
            Some(Self::new(prev))
        }
    }
}