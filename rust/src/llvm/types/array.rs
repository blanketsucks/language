use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct ArrayType {
    handle: *const ffi::LLVMType
}

impl ArrayType {
    pub(crate) fn new(handle: *const ffi::LLVMType) -> Self {
        Self { handle }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMType { self.handle }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpType(self.handle); }
        println!();
    }

    pub fn get(ty: llvm::Type, size: u32) -> Self {
        Self::new(unsafe { ffi::LLVMArrayType(ty.as_ptr(), size) })
    }

    pub fn get_element_type(&self) -> llvm::Type {
        llvm::Type::new(unsafe { ffi::LLVMGetElementType(self.handle) })
    }

    pub fn get_length(&self) -> u32 {
        unsafe { ffi::LLVMGetArrayLength(self.handle) }
    }

    pub fn ptr(&self) -> llvm::PointerType { 
        llvm::PointerType::new(unsafe { ffi::LLVMPointerType(self.handle, 0) })
    }
}