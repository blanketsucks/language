use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct StructType {
    handle: *const ffi::LLVMType
}

impl StructType {
    pub(crate) fn new(handle: *const ffi::LLVMType) -> Self {
        Self { handle }
    }

    pub fn get(tys: &[llvm::Type], packed: bool) -> Self {
        let mut types = tys.iter().map(|ty| ty.as_ptr()).collect::<Vec<_>>();
        let handle = unsafe {
            ffi::LLVMStructType(types.as_mut_ptr(), types.len() as u32, packed)
        };

        Self::new(handle)
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMType { self.handle }
    
    pub fn ptr(&self) -> llvm::PointerType { 
        llvm::PointerType::new(unsafe { ffi::LLVMPointerType(self.handle, 0) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpType(self.handle); }
        println!();
    }

    pub fn get_element_types(&self) -> Vec<llvm::Type> {
        let count = self.get_element_count();
        let mut types = Vec::with_capacity(count as usize);

        unsafe {
            ffi::LLVMGetStructElementTypes(self.handle, types.as_mut_ptr());
            types.set_len(count as usize);
        }

        types.into_iter().map(llvm::Type::new).collect()
    }

    pub fn get_element_count(&self) -> u32 {
        unsafe { ffi::LLVMCountStructElementTypes(self.handle) }
    }

    pub fn is_packed(&self) -> bool {
        unsafe { ffi::LLVMIsPackedStruct(self.handle) }
    }
}