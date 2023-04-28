
use crate::llvm;

use llvm::ffi;

pub mod function;
pub mod generic;
pub mod instruction;
pub mod metadata;

pub use function::*;
pub use generic::*;
pub use instruction::*;
pub use metadata::*;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct Value {
    value: *const ffi::LLVMValue
}

impl Value {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) };
        println!()
    }

    pub fn is_constant(&self) -> bool {
        unsafe { ffi::LLVMIsConstant(self.value) }
    }

    pub fn get_type(&self) -> llvm::Type {
        unsafe { llvm::Type::new(ffi::LLVMTypeOf(self.value)) }
    }

    pub fn is_basic_block(&self) -> bool {
        unsafe { ffi::LLVMValueIsBasicBlock(self.value) }
    }

    pub fn as_basic_block(&self) -> Option<llvm::BasicBlock> {
        if !self.is_basic_block() {
            return None;
        }

        Some(llvm::BasicBlock::from_ptr(unsafe { ffi::LLVMValueAsBasicBlock(self.value) }))
    }

    pub fn as_metadata(&self) -> llvm::Metadata {
        llvm::Metadata::new(unsafe { ffi::LLVMValueAsMetadata(self.value) as *const ffi::LLVMMetadata })
    }

    pub fn operands(&self) -> u32 {
        unsafe { ffi::LLVMGetNumOperands(self.value) }
    }

    pub fn get_operand(&self, index: u32) -> llvm::Value {
        llvm::Value::new(unsafe { ffi::LLVMGetOperand(self.value, index) })
    }

    pub fn set_operand(&self, index: u32, value: llvm::Value) {
        unsafe { ffi::LLVMSetOperand(self.value, index, value.as_ptr()) }
    }

    pub fn get_name(&self) -> &'static str {
        let mut len = 0;
        unsafe { ffi::to_str(ffi::LLVMGetValueName2(self.value, &mut len)) }
    }

    pub fn set_name(&self, name: &str) {
        let len = name.len();
        let cstr = std::ffi::CString::new(name).unwrap();

        unsafe { ffi::LLVMSetValueName2(self.value, cstr.as_ptr(), len) }
    }
}

macro_rules! impl_from {
    ($($type:ty),*) => {
        $(
            impl From<$type> for Value {
                fn from(value: $type) -> Self {
                    Self::new(value.as_ptr())
                }
            }
        )*
    }
}

impl_from!(
    llvm::Constant, 
    llvm::ConstantInt, 
    llvm::ConstantFP, 
    llvm::ConstantStruct,
    llvm::ConstantArray,
    llvm::Function,
    llvm::Instruction
);

impl TryFrom<Value> for llvm::Constant {
    type Error = ();

    fn try_from(value: Value) -> Result<Self, Self::Error> {
        if !value.is_constant() {
            return Err(());
        }

        Ok(Self::new(value.as_ptr()))
    }
}