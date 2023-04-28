use crate::llvm;

use llvm::ffi;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Context {
    ctx: *const ffi::LLVMContext
}

impl Context {
    pub fn new() -> Self {
        Self { ctx: unsafe { ffi::LLVMContextCreate() } }
    }

    pub fn global() -> Self {
        Self { ctx: unsafe { ffi::LLVMGetGlobalContext() } }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMContext { self.ctx }

    pub fn dispose(&self) {
        unsafe { ffi::LLVMContextDispose(self.ctx) }
    }
}

impl Default for Context {
    fn default() -> Self {
        Self::global()
    }
}