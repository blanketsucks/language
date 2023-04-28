use crate::llvm::{ffi, Type};

pub mod array;
pub mod fp;
pub mod int;
pub mod structs;

pub use array::ConstantArray;
pub use fp::ConstantFP;
pub use int::ConstantInt;
pub use structs::ConstantStruct;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct Constant {
    value: *const ffi::LLVMValue
}

impl Constant {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn null(ty: Type) -> Self {
        Self::new(unsafe { ffi::LLVMConstNull(ty.as_ptr()) })
    }

    pub fn null_ptr(ty: Type) -> Constant {
        if !ty.is_pointer() {
            panic!("Expected pointer type"); // Another option is to return Option<Constant> but idk
        }

        Self::new(unsafe { ffi::LLVMConstPointerNull(ty.as_ptr()) })
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }
    pub fn is_null(&self) -> bool { unsafe { ffi::LLVMIsNull(self.value) } }
    pub fn is_undef(&self) -> bool { unsafe { ffi::LLVMIsUndef(self.value) } }

    pub fn get_type(&self) -> Type {
        unsafe { Type::new(ffi::LLVMTypeOf(self.value)) }
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) };
        println!()
    }
}

macro_rules! impl_from {
    ($(($ty:ident, $verify:ident, $name:literal)),*) => {
        $(
            impl TryFrom<Constant> for $ty {
                type Error = String;
    
                fn try_from(value: Constant) -> Result<Self, Self::Error> {
                    if !value.get_type().$verify() {
                        return Err(format!("Expected {} type", $name));
                    }
    
                    Ok(Self::new(value.as_ptr()))
                }
            }
    
            impl From<$ty> for Constant {
                fn from(ty: $ty) -> Self {
                    Self::new(ty.as_ptr())
                }
            }
        )*
    }
}

impl_from! {
    (ConstantInt, is_integer, "integer"),
    (ConstantFP, is_float, "float"),
    (ConstantStruct, is_struct, "struct"),
    (ConstantArray, is_array, "array")
}