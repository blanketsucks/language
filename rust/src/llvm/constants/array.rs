use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct ConstantArray {
    value: *const ffi::LLVMValue
}

impl ConstantArray {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn get(ty: llvm::ArrayType, values: &[llvm::Constant]) -> Self {
        let mut values = values.iter().map(|c| c.as_ptr()).collect::<Vec<_>>();
        Self::new(unsafe { ffi::LLVMConstArray(ty.as_ptr(), values.as_mut_ptr(), values.len() as u32) })
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }
    pub fn is_null(&self) -> bool { unsafe { ffi::LLVMIsNull(self.value) } }
    pub fn is_undef(&self) -> bool { unsafe { ffi::LLVMIsUndef(self.value) } }

    pub fn get_element(&self, index: u32) -> llvm::Constant {
        llvm::Constant::new(unsafe { ffi::LLVMGetAggregateElement(self.value, index) })
    }

    pub fn as_string(&self) -> &'static str {
        let mut _len = 0;
        unsafe { ffi::to_str(ffi::LLVMGetAsString(self.value, &mut _len)) }
    }

    pub fn get_type(&self) -> llvm::ArrayType {
        llvm::ArrayType::new(unsafe { ffi::LLVMTypeOf(self.value) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) }
        println!()
    }
}