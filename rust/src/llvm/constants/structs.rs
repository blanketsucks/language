use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct ConstantStruct {
    value: *const ffi::LLVMValue
}

impl ConstantStruct {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn get(ty: llvm::Type, values: &[llvm::Constant], packed: bool) -> Self {
        if !ty.is_struct() {
            panic!("Expected struct type");
        }

        let mut values = values.iter().map(|c| c.as_ptr()).collect::<Vec<_>>();
        Self::new(unsafe { ffi::LLVMConstStruct(values.as_mut_ptr(), values.len() as u32, packed) })
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }
    pub fn is_null(&self) -> bool { unsafe { ffi::LLVMIsNull(self.value) } }
    pub fn is_undef(&self) -> bool { unsafe { ffi::LLVMIsUndef(self.value) } }

    pub fn get_element(&self, index: u32) -> llvm::Constant {
        llvm::Constant::new(unsafe { ffi::LLVMGetAggregateElement(self.value, index) })
    }

    pub fn get_type(&self) -> llvm::StructType {
        llvm::StructType::new(unsafe { ffi::LLVMTypeOf(self.value) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) }
        println!()
    }
}
