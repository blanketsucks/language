use crate::llvm;

use llvm::ffi;

pub type CodeGenOptLevel = ffi::LLVMCodeGenOptLevel;
pub type RelocMode = ffi::LLVMRelocMode;
pub type CodeModel = ffi::LLVMCodeModel;
pub type CodeGenFileType = ffi::LLVMCodeGenFileType;
pub type ByteOrdering = ffi::LLVMByteOrdering;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct TargetData {
    data: *const ffi::LLVMTargetData
}

impl TargetData {
    pub(crate) fn new(data: *const ffi::LLVMTargetData) -> Self {
        Self { data }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMTargetData { self.data }

    pub fn byte_ordering(&self) -> ByteOrdering {
        unsafe { ffi::LLVMByteOrder(self.data) }
    }

    pub fn pointer_size(&self) -> usize {
        unsafe { ffi::LLVMPointerSize(self.data) as usize }
    }

    pub fn store_size_of(&self, ty: llvm::Type) -> usize {
        unsafe { ffi::LLVMStoreSizeOfType(self.data, ty.as_ptr()) as usize }
    }

    pub fn abi_size_of(&self, ty: llvm::Type) -> usize {
        unsafe { ffi::LLVMABISizeOfType(self.data, ty.as_ptr()) as usize }
    }

    pub fn abi_alignment_of(&self, ty: llvm::Type) -> usize {
        unsafe { ffi::LLVMABIAlignmentOfType(self.data, ty.as_ptr()) as usize }
    }

    pub fn call_frame_alignment_of(&self, ty: llvm::Type) -> usize {
        unsafe { ffi::LLVMCallFrameAlignmentOfType(self.data, ty.as_ptr()) as usize }
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub struct Target {
    target: *const ffi::LLVMTarget
}

impl Target {
    pub(crate) fn new(target: *const ffi::LLVMTarget) -> Self {
        Self { target }
    }

    pub fn from_name(name: &str) -> Option<Self> {
        let cstr = std::ffi::CString::new(name).unwrap();
        let target = unsafe { ffi::LLVMGetTargetFromName(cstr.as_ptr()) };

        if target.is_null() {
            None
        } else {
            Some(Self::new(target))
        }
    }

    pub fn from_triple(triple: &str) -> Result<Self, String> {
        let cstr = std::ffi::CString::new(triple).unwrap();

        let target = std::ptr::null_mut();
        let mut err = std::ptr::null_mut();

        let error = unsafe { 
            ffi::LLVMGetTargetFromTriple(cstr.as_ptr(), target, &mut err)
        };

        if error {
            let err = unsafe { std::ffi::CStr::from_ptr(err) };
            Err(err.to_string_lossy().into_owned())
        } else {
            unsafe { Ok(Self::new(*target)) }
        }
    }

    pub fn from_default_triple() -> Result<Self, String> {
        let mut target: *const ffi::LLVMTarget = std::ptr::null_mut();
        let mut err = std::ptr::null_mut();

        let error = unsafe {
            ffi::LLVMGetTargetFromTriple(
                ffi::LLVMGetDefaultTargetTriple(), &mut target, &mut err
            )
        };

        if error {
            let err = unsafe { std::ffi::CStr::from_ptr(err) };
            Err(err.to_string_lossy().into_owned())
        } else {
            Ok(Self::new(target))
        }
    }

    pub fn name(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetTargetName(self.target)) }
    }

    pub fn description(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetTargetDescription(self.target)) }
    }

    pub fn has_jit(&self) -> bool {
        unsafe { ffi::LLVMTargetHasJIT(self.target) }
    }

    pub fn has_target_machine(&self) -> bool {
        unsafe { ffi::LLVMTargetHasTargetMachine(self.target) }
    }

    pub fn has_asm_backend(&self) -> bool {
        unsafe { ffi::LLVMTargetHasAsmBackend(self.target) }
    }

    pub fn create_target_machine(
        self,
        triple: &str,
        cpu: &str,
        features: &str,
        level: CodeGenOptLevel,
        reloc: RelocMode,
        model: CodeModel,
    ) -> TargetMachine {
        unsafe {
            let target = ffi::LLVMCreateTargetMachine(
                self.target,
                ffi::to_cstr(triple),
                ffi::to_cstr(cpu),
                ffi::to_cstr(features),
                level,
                reloc,
                model
            );

            TargetMachine::new(target)
        }
    }
}

impl std::fmt::Debug for Target {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("Target")
            .field("name", &self.name())
            .field("description", &self.description())
            .field("has_jit", &self.has_jit())
            .field("has_target_machine", &self.has_target_machine())
            .field("has_asm_backend", &self.has_asm_backend())
            .finish()
    }
}

#[derive(Copy, Clone, PartialEq, Eq, Hash)]
pub struct TargetMachine {
    machine: *const ffi::LLVMTargetMachine
}

impl TargetMachine {
    pub(crate) fn new(machine: *const ffi::LLVMTargetMachine) -> Self {
        Self { machine }
    }

    pub fn target(&self) -> Target {
        unsafe { Target::new(ffi::LLVMGetTargetMachineTarget(self.machine)) }
    }

    pub fn triple(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetTargetMachineTriple(self.machine)) }
    }

    pub fn cpu(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetTargetMachineCPU(self.machine)) }
    }

    pub fn features(&self) -> &'static str {
        unsafe { ffi::to_str(ffi::LLVMGetTargetMachineFeatureString(self.machine)) }
    }

    pub fn create_data_layout(&self) -> TargetData {
        TargetData::new(unsafe { ffi::LLVMCreateTargetDataLayout(self.machine) })
    }

    pub fn emit_to_file(
        self, module: &llvm::Module, filename: &str, codegen: CodeGenFileType
    ) -> Result<(), String> {
        let filename = ffi::to_cstr(filename);
        let mut err = std::ptr::null_mut();

        let error = unsafe {
            ffi::LLVMTargetMachineEmitToFile(
                self.machine,
                module.as_ptr(),
                filename,
                codegen,
                &mut err
            )
        };

        if error {
            let err = unsafe { ffi::to_str(err) };
            Err(err.to_string())
        } else {
            Ok(())
        }
    }
}

impl std::fmt::Debug for TargetMachine {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("TargetMachine")
            .field("triple", &self.triple())
            .field("cpu", &self.cpu())
            .finish()
    }
}