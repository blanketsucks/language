use crate::llvm;

use llvm::ffi;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct GenericValue {
    value: *const ffi::LLVMGenericValue
}

impl GenericValue {
    pub(crate) fn new(value: *const ffi::LLVMGenericValue) -> Self {
        Self { value }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMGenericValue { self.value }

    pub fn int(ty: llvm::Type, value: u64) -> Self {
        Self::new(unsafe { ffi::LLVMCreateGenericValueOfInt(ty.as_ptr(), value, false) })
    }

    pub fn float(ty: llvm::Type, value: f64) -> Self {
        Self::new(unsafe { ffi::LLVMCreateGenericValueOfFloat(ty.as_ptr(), value) })
    }

    pub fn pointer<T>(value: &mut T) -> Self {
        Self::new(unsafe { ffi::LLVMCreateGenericValueOfPointer(value as *mut T as *mut _) })
    }

    pub fn to_int(&self, signed: bool) -> u64 {
        unsafe { ffi::LLVMGenericValueToInt(self.value, signed) }
    }

    pub fn to_float(&self, ty: llvm::Type) -> f64 {
        unsafe { ffi::LLVMGenericValueToFloat(ty.as_ptr(), self.value) }
    }

    pub unsafe fn to_pointer<T>(&self) -> *mut T {
        ffi::LLVMGenericValueToPointer(self.value) as *mut T
    }

    pub fn dispose(&self) {
        unsafe { ffi::LLVMDisposeGenericValue(self.value) }
    }
}

impl Drop for GenericValue {
    fn drop(&mut self) {
        self.dispose();
    }
}
