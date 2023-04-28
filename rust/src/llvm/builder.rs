use crate::llvm;

use llvm::ffi;

use std::ffi::CStr;
use libc;

const EMPTY_C_STR: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"\0") };
const UNNAMED: *const libc::c_char = EMPTY_C_STR.as_ptr();

macro_rules! _builder_value_inst {
    ($($name:ident($($arg:ident),*) => $func:ident),+ $(,)?) => {
        $(pub fn $name(&mut self, $($arg: llvm::Value),*) -> llvm::Value {
            unsafe {
                llvm::Value::new(ffi::$func(self.builder, $($arg.as_ptr(),)* UNNAMED))
            }
        })+
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Builder {
    builder: *const ffi::LLVMBuilder
}

impl Builder {
    pub fn new(ctx: Option<&llvm::Context>) -> Self {
        let ptr = match ctx {
            Some(ctx) => unsafe { ffi::LLVMCreateBuilderInContext(ctx.as_ptr()) },
            None => unsafe { ffi::LLVMCreateBuilder() }
        };

        Self { builder: ptr }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMBuilder { self.builder }

    pub fn dispose(&self) {
        unsafe { ffi::LLVMDisposeBuilder(self.builder) }
    }

    pub fn position_at_end(&self, block: llvm::BasicBlock) {
        unsafe { ffi::LLVMPositionBuilderAtEnd(self.builder, block.as_ptr()) }
    }

    pub fn get_insert_block(&self) -> llvm::BasicBlock {
        unsafe { llvm::BasicBlock::from_ptr(ffi::LLVMGetInsertBlock(self.builder)) }
    }

    pub fn str(&mut self, value: &str, as_ptr: bool) -> llvm::Value {
        let ptr = ffi::to_cstr(value);
        if !as_ptr {
            return unsafe { llvm::Value::new(ffi::LLVMBuildGlobalString(
                self.builder, ptr, UNNAMED
            )) };
        }

        unsafe { llvm::Value::new(ffi::LLVMBuildGlobalStringPtr(self.builder, ptr, UNNAMED)) }
    }

    pub fn ret(&mut self, value: Option<llvm::Value>) {
        match value {
            Some(value) => unsafe { ffi::LLVMBuildRet(self.builder, value.as_ptr()) },
            None => unsafe { ffi::LLVMBuildRetVoid(self.builder) }
        };
    }

    pub fn br(&mut self, block: llvm::BasicBlock) {
        unsafe { ffi::LLVMBuildBr(self.builder, block.as_ptr()); }
    }

    pub fn cond_br(
        &mut self, cond: llvm::Value, then: llvm::BasicBlock, otherwise: llvm::BasicBlock
    ) {
        unsafe { ffi::LLVMBuildCondBr(
            self.builder, cond.as_ptr(), then.as_ptr(), otherwise.as_ptr()
        ); }
    }

    pub fn call(&mut self, func: llvm::Value, args: &mut [llvm::Value]) -> llvm::Value {
        unsafe {
            llvm::Value::new(ffi::LLVMBuildCall(
                self.builder, func.as_ptr(), args.as_mut_ptr() as *mut _,
                args.len() as libc::c_uint, UNNAMED
            ))
        }
    }

    _builder_value_inst! {
        add(a, b) => LLVMBuildAdd,
        fadd(a, b) => LLVMBuildFAdd,
        sub(a, b) => LLVMBuildSub,
        fsub(a, b) => LLVMBuildFSub,
        mul(a, b) => LLVMBuildMul,
        fmul(a, b) => LLVMBuildFMul,
        udiv(a, b) => LLVMBuildUDiv,
        sdiv(a, b) => LLVMBuildSDiv,
        exactsdiv(a, b) => LLVMBuildExactSDiv,
        fdiv(a, b) => LLVMBuildFDiv,
        urem(a, b) => LLVMBuildURem,
        srem(a, b) => LLVMBuildSRem,
        frem(a, b) => LLVMBuildFRem,
        shl(a, b) => LLVMBuildShl,
        lshr(a, b) => LLVMBuildLShr,
        ashr(a, b) => LLVMBuildAShr,
        and(a, b) => LLVMBuildAnd,
        or(a, b) => LLVMBuildOr,
        xor(a, b) => LLVMBuildXor,
        neg(x) => LLVMBuildNeg,
        fneg(x) => LLVMBuildFNeg,
        not(x) => LLVMBuildNot
    }

    pub fn alloca(&mut self, ty: llvm::Type) -> llvm::Value {
        unsafe { llvm::Value::new(ffi::LLVMBuildAlloca(self.builder, ty.as_ptr(), UNNAMED)) }
    }

    pub fn load(&mut self, ptr: llvm::Value) -> llvm::Value {
        unsafe { llvm::Value::new(ffi::LLVMBuildLoad(self.builder, ptr.as_ptr(), UNNAMED)) }
    }

    pub fn store(&mut self, value: llvm::Value, ptr: llvm::Value) -> llvm::Value {
        unsafe { llvm::Value::new(ffi::LLVMBuildStore(self.builder, value.as_ptr(), ptr.as_ptr())) }
    }

    pub fn gep(&mut self, ptr: llvm::Value, indices: &[llvm::Value]) -> llvm::Value {
        unsafe {
            llvm::Value::new(ffi::LLVMBuildGEP(
                self.builder, 
                ptr.as_ptr(), 
                indices.as_ptr() as *mut _, 
                indices.len() as u32, 
                UNNAMED
            ))
        }
    }
}
