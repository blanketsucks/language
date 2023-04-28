use crate::llvm;

use llvm::ffi;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct MemoryBuffer {
    buffer: *const ffi::LLVMMemoryBuffer,
}

impl MemoryBuffer {
    pub fn new(buffer: *const ffi::LLVMMemoryBuffer) -> Self {
        Self { buffer }
    }

    pub fn create_from_file(path: &str) -> Result<Self, String> {
        let mut buffer: *const ffi::LLVMMemoryBuffer = std::ptr::null_mut();
        let mut err = std::ptr::null_mut();

        let path = ffi::to_cstr(path);
        let error = unsafe { ffi::LLVMCreateMemoryBufferWithContentsOfFile(
            path, &mut buffer, &mut err
        ) };

        if !error {
            Ok(Self::new(buffer))
        } else {
            Err(unsafe { ffi::to_str(err) }.to_string())
        }
    }

    pub fn create_from_stdin() -> Result<Self, String> {
        let mut buffer: *const ffi::LLVMMemoryBuffer = std::ptr::null_mut();
        let mut err = std::ptr::null_mut();

        let error = unsafe { ffi::LLVMCreateMemoryBufferWithSTDIN(
            &mut buffer, &mut err
        ) };

        if !error {
            Ok(Self::new(buffer))
        } else {
            Err(unsafe { ffi::to_str(err) }.to_string())
        }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMMemoryBuffer { self.buffer }

    pub fn as_slice(&self) -> &[u8] {
        unsafe { std::slice::from_raw_parts(
            ffi::LLVMGetBufferStart(self.buffer) as *const u8, self.size()
        ) }
    }

    pub fn size(&self) -> usize {
        unsafe { ffi::LLVMGetBufferSize(self.buffer) }
    }
}