use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct ConstantFP {
    value: *const ffi::LLVMValue
}

impl ConstantFP {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn get(ty: llvm::Type, value: f64) -> Self {
        if !ty.is_floating_point() {
            panic!("Expected floating point type");
        }

        Self::new(unsafe { ffi::LLVMConstReal(ty.as_ptr(), value) })
    }

    pub fn get_str(ty: llvm::Type, value: &str) -> Self {
        if !ty.is_floating_point() {
            panic!("Expected floating point type");
        }

        let cstr = std::ffi::CString::new(value).unwrap();
        let ptr = unsafe { 
            ffi::LLVMConstRealOfString(ty.as_ptr(), cstr.as_ptr())
        };

        Self::new(ptr)
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }
    pub fn is_null(&self) -> bool { unsafe { ffi::LLVMIsNull(self.value) } }
    pub fn is_undef(&self) -> bool { unsafe { ffi::LLVMIsUndef(self.value) } }

    pub fn get_double(&self) -> (bool, f64) {
        let mut loses_info = false;
        let value = unsafe { ffi::LLVMConstRealGetDouble(self.value, &mut loses_info) };

        (loses_info, value)
    }

    pub fn get_type(&self) -> llvm::Type {
        llvm::Type::new(unsafe { ffi::LLVMTypeOf(self.value) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) }
        println!()
    }
}

impl From<f64> for ConstantFP {
    fn from(value: f64) -> Self {
        Self::get(llvm::Type::double(), value)
    }
}

impl From<f32> for ConstantFP {
    fn from(value: f32) -> Self {
        Self::get(llvm::Type::float(), value as f64)
    }
}

impl From<&str> for ConstantFP {
    fn from(value: &str) -> Self {
        Self::get_str(llvm::Type::double(), value)
    }
}