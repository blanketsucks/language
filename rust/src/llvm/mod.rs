pub mod ffi;
pub mod constants;
pub mod types;
pub mod module;
pub mod context;
pub mod values;
pub mod target;
pub mod builder;
pub mod basic_blocks;
pub mod execution_engine;
pub mod binary;
pub mod memory_buffer;

pub use constants::{Constant, ConstantInt, ConstantFP, ConstantArray, ConstantStruct};
pub use types::{Type, IntegerType, FunctionType, StructType, ArrayType, PointerType};
pub use values::{Value, Function, Instruction, Metadata, GenericValue, Linkage, Visibility, VerifyAction};
pub use module::*;
pub use context::*;
pub use target::*;
pub use builder::*;
pub use basic_blocks::*;
pub use execution_engine::*;
pub use target::*;
pub use memory_buffer::*;
pub use binary::*;

pub fn init() {
    unsafe { ffi::LLVMInitializeAll(); }
}

pub fn shutdown() {
    unsafe { ffi::LLVMShutdown(); }
}

pub fn default_target_triple() -> &'static str {
    unsafe { ffi::to_str(ffi::LLVMGetDefaultTargetTriple()) }
}

pub fn default_cpu() -> &'static str {
    unsafe { ffi::to_str(ffi::LLVMGetHostCPUName()) }
}

pub fn default_cpu_features() -> &'static str {
    unsafe { ffi::to_str(ffi::LLVMGetHostCPUFeatures()) }
}