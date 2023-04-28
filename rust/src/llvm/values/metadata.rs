use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct Metadata {
    value: *const ffi::LLVMMetadata
}

impl Metadata {
    pub(crate) fn new(value: *const ffi::LLVMMetadata) -> Self {
        Self { value }
    }

    pub fn int(value: u64, size: u32) -> Metadata {
        let value: llvm::Value = llvm::ConstantInt::get(
            llvm::IntegerType::get(size),
            value,
            false
        ).into();

        Metadata::from(value)
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMMetadata { self.value }

    pub fn as_value(&self, ctx: llvm::Context) -> llvm::Value {
        llvm::Value::new(unsafe { ffi::LLVMMetadataAsValue(ctx.as_ptr(), self.value) })
    }

    pub fn get_string(&self) -> &'static str {
        let mut len = 0;
        unsafe {
            ffi::to_str(ffi::LLVMGetMDString(self.value as *const ffi::LLVMValue, &mut len))
        }
    }
}

impl From<llvm::Value> for Metadata {
    fn from(value: llvm::Value) -> Self {
        value.as_metadata()
    }
}
