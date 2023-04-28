use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct FunctionType {
    handle: *const ffi::LLVMType
}

impl FunctionType {
    pub(crate) fn new(handle: *const ffi::LLVMType) -> Self {
        Self { handle }
    }

    pub fn get(ret: llvm::Type, params: &[llvm::Type], vararg: bool) -> Self {
        let mut params = params.iter().map(|ty| ty.as_ptr()).collect::<Vec<_>>();
        let handle = unsafe {
            ffi::LLVMFunctionType(
                ret.as_ptr(), params.as_mut_ptr(), params.len() as u32, vararg
            )
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

    pub fn get_return_type(&self) -> llvm::Type {
        llvm::Type::new(unsafe { ffi::LLVMGetReturnType(self.handle) })
    }

    pub fn get_param_count(&self) -> u32 {
        unsafe { ffi::LLVMCountParamTypes(self.handle) }
    }

    pub fn get_param_types(&self) -> Vec<llvm::Type> {
        let count = self.get_param_count();
        let mut types = Vec::with_capacity(count as usize);

        unsafe {
            ffi::LLVMGetParamTypes(self.handle, types.as_mut_ptr());
            types.set_len(count as usize);
        }

        types.into_iter().map(llvm::Type::new).collect()
    }

    pub fn is_vararg(&self) -> bool {
        unsafe { ffi::LLVMIsFunctionVarArg(self.handle) }
    }
}