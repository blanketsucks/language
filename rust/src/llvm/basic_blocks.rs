use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct BasicBlock {
    block: *const ffi::LLVMBasicBlock
}

impl BasicBlock {
    pub fn new(name: &str, ctx: Option<llvm::Context>) -> Self {
        let ptr = match ctx {
            Some(ctx) => unsafe { ffi::LLVMCreateBasicBlockInContext(ctx.as_ptr(), ffi::to_cstr(name)) },
            None => unsafe { ffi::LLVMCreateBasicBlock(ffi::to_cstr(name)) }
        };

        Self { block: ptr }
    }

    pub(crate) fn from_ptr(ptr: *const ffi::LLVMBasicBlock) -> Self {
        Self { block: ptr }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMBasicBlock { self.block }

    pub fn get_parent(&self) -> llvm::Function {
        unsafe { llvm::Function::new(ffi::LLVMGetBasicBlockParent(self.block)) }
    }

    pub fn delete(&self) {
        unsafe { ffi::LLVMDeleteBasicBlock(self.block) }
    }

    pub fn remove_from_parent(&self) {
        unsafe { ffi::LLVMRemoveBasicBlockFromParent(self.block) }
    }

    pub fn get_name(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetBasicBlockName(self.block)) }
    }

    pub fn get_next_basic_block(&self) -> Option<Self> {
        let ptr = unsafe { ffi::LLVMGetNextBasicBlock(self.block) };
        if ptr.is_null() {
            return None;
        }

        Some(Self::from_ptr(ptr))
    }

    pub fn get_previous_basic_block(&self) -> Option<Self> {
        let ptr = unsafe { ffi::LLVMGetPreviousBasicBlock(self.block) };
        if ptr.is_null() {
            return None;
        }

        Some(Self::from_ptr(ptr))
    }

    pub fn get_first_instruction(&self) -> Option<llvm::Instruction> {
        let ptr = unsafe { ffi::LLVMGetFirstInstruction(self.block) };
        if ptr.is_null() {
            return None;
        }

        Some(llvm::Instruction::new(ptr))
    }

    pub fn get_last_instruction(&self) -> Option<llvm::Instruction> {
        let ptr = unsafe { ffi::LLVMGetLastInstruction(self.block) };
        if ptr.is_null() {
            return None;
        }

        Some(llvm::Instruction::new(ptr))
    }

    pub fn instructions(&self) -> InstructionIterator {
        InstructionIterator {
            current: self.get_first_instruction()
        }
    }
}

impl TryFrom<llvm::Value> for BasicBlock {
    type Error = ();

    fn try_from(value: llvm::Value) -> Result<Self, Self::Error> {
        value.as_basic_block().ok_or(())
    }
}

pub struct InstructionIterator {
    current: Option<llvm::Instruction>
}

impl Iterator for InstructionIterator {
    type Item = llvm::Instruction;

    fn next(&mut self) -> Option<Self::Item> {
        let current = self.current?;
        self.current = current.get_next_instruction();

        Some(current)
    }
}