use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct IntegerType {
    handle: *const ffi::LLVMType
}

impl IntegerType {
    pub(crate) fn new(handle: *const ffi::LLVMType) -> Self {
        Self { handle }
    }

    pub fn get(bits: u32) -> Self {
        Self::new(unsafe { ffi::LLVMIntType(bits) })
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMType { self.handle }
    
    pub fn ptr(&self) -> llvm::PointerType { 
        llvm::PointerType::new(unsafe { ffi::LLVMPointerType(self.handle, 0) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpType(self.handle); }
        println!();
    }

    pub fn get_width(&self) -> u32 {
        unsafe { ffi::LLVMGetIntTypeWidth(self.handle) }
    }

}
