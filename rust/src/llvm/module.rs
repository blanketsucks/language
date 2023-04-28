use crate::llvm;

use llvm::ffi;

pub type FlagBehavior = ffi::LLVMModFlagBehavior;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum PICLevel {
    Not,
    Small,
    Big
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum PIELevel {
    Default,
    Small,
    Large
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Module {
    module: *const ffi::LLVMModule
}

// Module (along with Context and Builder) can't implement Drop because if we can't control the drop order.
// If the module is tied to a context and the context is dropped first, it will cause 
// a segmentation fault when trying to drop Module.
impl Module {
    pub fn new(id: &str, ctx: Option<&llvm::Context>) -> Self {
        let cstr = ffi::to_cstr(id);
        let module = match ctx {
            Some(ctx) => unsafe { ffi::LLVMModuleCreateWithNameInContext(cstr, ctx.as_ptr()) },
            None => unsafe { ffi::LLVMModuleCreateWithName(cstr) }
        };

        Self { module }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMModule { self.module }

    pub fn dispose(&self) {
        unsafe { ffi::LLVMDisposeModule(self.module) }
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpModule(self.module) }
    }

    pub fn get_identifier(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetModuleIdentifier(self.module)) }
    }

    pub fn set_identifier(&mut self, id: &str) {
        let len = id.len();
        let cstr = std::ffi::CString::new(id).unwrap();

        unsafe { ffi::LLVMSetModuleIdentifier(self.module, cstr.as_ptr(), len) }
    }

    pub fn get_source_filename(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetSourceFileName(self.module)) }   
    }

    pub fn set_source_filename(&mut self, filename: &str) {
        let len = filename.len();
        let cstr = std::ffi::CString::new(filename).unwrap();

        unsafe { ffi::LLVMSetSourceFileName(self.module, cstr.as_ptr(), len) }
    }

    pub fn get_data_layout(&self) -> llvm::TargetData {
        llvm::TargetData::new(unsafe { ffi::LLVMGetDataLayout(self.module) })
    }

    pub fn set_data_layout(&mut self, layout: llvm::TargetData) {
        unsafe { ffi::LLVMSetDataLayout(self.module, layout.as_ptr()) }
    }

    pub fn get_target_triple(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetTarget(self.module)) }
    }

    pub fn set_target_triple(&mut self, triple: &str) {
        unsafe { ffi::LLVMSetTarget(self.module, ffi::to_cstr(triple)) }
    }

    pub fn get_type_by_name(&self, name: &str) -> Option<llvm::Type> {
        let ptr = unsafe { ffi::LLVMGetTypeByName(self.module, ffi::to_cstr(name)) };

        if ptr.is_null() {
            None
        } else {
            Some(llvm::Type::new(ptr))
        }
    }

    pub fn add_function(&mut self, name: &str, ty: llvm::FunctionType) -> llvm::Function {
        let cstr = std::ffi::CString::new(name).unwrap();
        let ptr = unsafe { ffi::LLVMAddFunction(self.module, cstr.as_ptr(), ty.as_ptr()) };

        llvm::Function::new(ptr)
    }

    pub fn get_function(&self, name: &str) -> Option<llvm::Function> {
        let cstr = std::ffi::CString::new(name).unwrap();
        let ptr = unsafe { ffi::LLVMGetNamedFunction(self.module, cstr.as_ptr()) };

        if ptr.is_null() {
            None
        } else {
            Some(llvm::Function::new(ptr))
        }
    }

    pub fn functions(&self) -> FunctionIterator {
        let ptr = unsafe { ffi::LLVMGetFirstFunction(self.module) };
        FunctionIterator { current: ptr }
    }

    pub fn add_flag(&mut self, name: &str, behavior: FlagBehavior, value: llvm::Metadata) {
        let len = name.len();
        let cstr = ffi::to_cstr(name);

        unsafe { ffi::LLVMAddModuleFlag(
            self.module, 
            behavior,
            cstr, 
            len as u32, 
            value.as_ptr()
        ) }
    }

    pub fn set_pie_level(&mut self, level: PIELevel) {
        let value = match level {
            PIELevel::Default => 0,
            PIELevel::Small => 1,
            PIELevel::Large => 2
        };

        self.add_flag("PIE Level", FlagBehavior::AppendUnique, llvm::Metadata::int(value, 32));
    }

    pub fn set_pic_level(&mut self, level: PICLevel) {
        let value = match level {
            PICLevel::Not => 0,
            PICLevel::Small => 1,
            PICLevel::Big => 2
        };

        self.add_flag("PIC Level", FlagBehavior::AppendUnique, llvm::Metadata::int(value, 32));
    }

    pub fn verify(&self, action: llvm::VerifyAction) -> Result<(), String> {
        let mut error = std::ptr::null_mut();
        let err = unsafe { ffi::LLVMVerifyModule(
            self.module, 
            match action {
                llvm::VerifyAction::Abort => ffi::LLVMVerifierFailureAction::ReturnStatusAction,
                llvm::VerifyAction::Print => ffi::LLVMVerifierFailureAction::PrintMessageAction,
                llvm::VerifyAction::Return => ffi::LLVMVerifierFailureAction::ReturnStatusAction
            },
            &mut error
        ) };

        if err {
            let error = unsafe { ffi::to_str(error) };
            Err(error.to_string())
        } else {
            Ok(())
        }
    }

    pub fn create_execution_engine(&self) -> Result<llvm::ExecutionEngine, String> {
        let mut engine: *const ffi::LLVMExecutionEngine = std::ptr::null_mut();
        let mut error = std::ptr::null_mut();

        let err = unsafe { ffi::LLVMCreateExecutionEngineForModule(
            &mut engine, 
            self.module, 
            &mut error
        ) };

        if err {
            let error = unsafe { ffi::to_str(error) };
            Err(error.to_string())
        } else {
            Ok(llvm::ExecutionEngine::new(engine))
        }
    }
}

pub struct FunctionIterator {
    current: *const ffi::LLVMValue
}

impl Iterator for FunctionIterator {
    type Item = llvm::Function;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current.is_null() {
            None
        } else {
            let function = llvm::Function::new(self.current);
            self.current = unsafe { ffi::LLVMGetNextFunction(self.current) };

            Some(function)
        }
    }
}
