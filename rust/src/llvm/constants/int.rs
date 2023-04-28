use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct ConstantInt {
    value: *const ffi::LLVMValue
}

impl ConstantInt {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn get(ty: llvm::IntegerType, value: u64, sign_extend: bool) -> Self {
        Self::new(unsafe { ffi::LLVMConstInt(ty.as_ptr(), value, sign_extend) })
    }

    pub fn get_str(ty: llvm::IntegerType, value: &str, radix: u8) -> Self {
        let cstr = std::ffi::CString::new(value).unwrap();
        let ptr = unsafe { 
            ffi::LLVMConstIntOfString(ty.as_ptr(), cstr.as_ptr(), radix)
        };

        Self::new(ptr)
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }
    pub fn is_null(&self) -> bool { unsafe { ffi::LLVMIsNull(self.value) } }
    pub fn is_undef(&self) -> bool { unsafe { ffi::LLVMIsUndef(self.value) } }

    pub fn get_zext_value(&self) -> u64 {
        unsafe { ffi::LLVMConstIntGetZExtValue(self.value) }
    }

    pub fn get_sext_value(&self) -> i64 {
        unsafe { ffi::LLVMConstIntGetSExtValue(self.value) }
    }

    pub fn get_type(&self) -> llvm::IntegerType {
        llvm::IntegerType::new(unsafe { ffi::LLVMTypeOf(self.value) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) }
        println!()
    }
}

macro_rules! impl_int_from {
    ($(($ty:ty, $sign_extend:literal)),*) => {
        $(
            impl From<$ty> for ConstantInt {
                fn from(value: $ty) -> Self {
                    Self::get(llvm::IntegerType::get(std::mem::size_of::<$ty>() as u32 * 8).into(), value as u64, $sign_extend)
                }
            }
        )*
    };
}

impl_int_from! {
    (u8, false),
    (u16, false),
    (u32, false),
    (u64, false),
    (u128, false),
    (i8, true),
    (i16, true),
    (i32, true),
    (i64, true),
    (i128, true)
}
