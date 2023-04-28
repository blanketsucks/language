use crate::llvm;

use llvm::ffi;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct ExecutionEngine {
    engine: *const ffi::LLVMExecutionEngine
}

impl ExecutionEngine {
    pub(crate) fn new(engine: *const ffi::LLVMExecutionEngine) -> Self {
        Self { engine }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMExecutionEngine { self.engine }

    pub unsafe fn run_function(
        &self, function: llvm::Function, args: &[&llvm::GenericValue]
    ) -> llvm::GenericValue {
        let mut args = args.iter().map(|arg| arg.as_ptr()).collect::<Vec<_>>();
        let value = ffi::LLVMRunFunction(
            self.engine, function.as_ptr(), args.len() as u32, args.as_mut_ptr()
        );

        llvm::GenericValue::new(value)
    }

    pub unsafe fn run_function_as_main(
        &self, function: llvm::Function, args: &[&str], envp: &[&str]
    ) -> i32 {
        let mut args = args.iter().map(|arg| ffi::to_cstr(arg)).collect::<Vec<_>>();
        let mut envp = envp.iter().map(|env| ffi::to_cstr(env)).collect::<Vec<_>>();

        unsafe {
            ffi::LLVMRunFunctionAsMain(
                self.engine, 
                function.as_ptr(), 
                args.len() as u32, 
                args.as_mut_ptr(), 
                envp.as_mut_ptr()
            )
        }
    }

    pub fn add_module(&self, module: &llvm::Module) {
        unsafe { ffi::LLVMAddModule(self.engine, module.as_ptr()); }
    }

    pub fn get_function_address(&self, name: &str) -> u64 {
        unsafe { ffi::LLVMGetFunctionAddress(self.engine, ffi::to_cstr(name)) }
    }

    pub unsafe fn get_function<T>(&self, name: &str) -> Option<T> {
        let address = self.get_function_address(name);
        assert_eq!(std::mem::size_of::<T>(), std::mem::size_of::<u64>(), "Function pointer size mismatch");

        if address == 0 {
            None
        } else {
            Some(std::mem::transmute_copy(&address))
        }
    }

    pub fn get_global_value_address(&self, name: &str) -> u64 {
        unsafe { ffi::LLVMGetGlobalValueAddress(self.engine, ffi::to_cstr(name)) }
    }

    pub unsafe fn run_static_constructors(&self) {
        ffi::LLVMRunStaticConstructors(self.engine);
    }

    pub unsafe fn run_static_destructors(&self) {
        ffi::LLVMRunStaticDestructors(self.engine);
    }

    pub fn dispose(&self) {
        unsafe { ffi::LLVMDisposeExecutionEngine(self.engine) }
    }
}
