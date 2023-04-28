use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct PointerType {
    handle: *const ffi::LLVMType
}

impl PointerType {
    pub(crate) fn new(handle: *const ffi::LLVMType) -> Self {
        Self { handle }
    }

    pub fn get(ty: llvm::Type, addr_space: u32) -> Self {
        Self::new(unsafe { ffi::LLVMPointerType(ty.as_ptr(), addr_space) })
    }

    pub fn get_address_space(&self) -> u32 {
        unsafe { ffi::LLVMGetPointerAddressSpace(self.handle) }
    }

    pub fn get_element_type(&self) -> llvm::Type {
        llvm::Type::new(unsafe { ffi::LLVMGetElementType(self.handle) })
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMType { self.handle }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpType(self.handle); }
        println!();
    }

    pub fn ptr(&self) -> llvm::PointerType { 
        llvm::PointerType::new(unsafe { ffi::LLVMPointerType(self.handle, 0) })
    }
}
