use crate::llvm;

use llvm::ffi;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum Linkage {
    External,
    AvailableExternally,
    LinkOnceAny,
    LinkOnceODR,
    LinkOnceODRAutoHide,
    WeakAny,
    WeakODR,
    Appending,
    Internal,
    Private,
    ExternalWeak,
    Common,
    LinkerPrivate,
    LinkerPrivateWeak,
    DLLImport,
    DLLExport,
    Ghost
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum Visibility {
    Default,
    Hidden,
    Protected
}

pub type CallConv = ffi::LLVMCallConv;

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum VerifyAction {
    Abort,
    Print,
    Return
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct Function {
    value: *const ffi::LLVMValue
}

impl Function {
    pub(crate) fn new(value: *const ffi::LLVMValue) -> Self {
        Self { value }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMValue { self.value }

    pub fn get_type(&self) -> llvm::PointerType {
        unsafe { llvm::PointerType::new(ffi::LLVMTypeOf(self.value)) }
    }

    pub fn get_function_type(&self) -> llvm::FunctionType {
        unsafe { llvm::FunctionType::new(ffi::LLVMGlobalGetValueType(self.value)) }
    }

    pub fn get_name(&self) -> &'static str {
        let mut _len = 0;
        unsafe { ffi::to_str(ffi::LLVMGetValueName2(self.value, &mut _len)) }
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpValue(self.value) };
        println!();
    }

    pub fn delete(&mut self) {
        unsafe { ffi::LLVMDeleteFunction(self.value) }
    }

    pub fn has_personality(&self) -> bool {
        unsafe { ffi::LLVMHasPersonalityFn(self.value) }
    }

    pub fn get_personality(&self) -> Option<Function> {
        if !self.has_personality() {
            return None;
        }

        let ptr = unsafe { ffi::LLVMGetPersonalityFn(self.value) };
        Some(Function::new(ptr))
    }

    pub fn set_personality(&mut self, personality: Function) {
        unsafe { ffi::LLVMSetPersonalityFn(self.value, personality.as_ptr()) }
    }

    pub fn get_gc(&self) -> Option<&str> {
        let ptr = unsafe { ffi::LLVMGetGC(self.value) };
        if ptr.is_null() {
            None
        } else {
            Some(unsafe { std::ffi::CStr::from_ptr(ptr) }.to_str().unwrap())
        }
    }

    pub fn set_gc(&mut self, gc: &str) {
        let cstr = std::ffi::CString::new(gc).unwrap();
        unsafe { ffi::LLVMSetGC(self.value, cstr.as_ptr() as *const i8) }
    }

    pub fn get_linkage(&self) -> Linkage {
        use Linkage::*;

        match unsafe { ffi::LLVMGetLinkage(self.value) } {
            ffi::LLVMLinkage::ExternalLinkage => External,
            ffi::LLVMLinkage::AvailableExternallyLinkage => AvailableExternally,
            ffi::LLVMLinkage::LinkOnceAnyLinkage => LinkOnceAny,
            ffi::LLVMLinkage::LinkOnceODRLinkage => LinkOnceODR,
            ffi::LLVMLinkage::LinkOnceODRAutoHideLinkage => LinkOnceODRAutoHide,
            ffi::LLVMLinkage::WeakAnyLinkage => WeakAny,
            ffi::LLVMLinkage::WeakODRLinkage => WeakODR,
            ffi::LLVMLinkage::AppendingLinkage => Appending,
            ffi::LLVMLinkage::InternalLinkage => Internal,
            ffi::LLVMLinkage::PrivateLinkage => Private,
            ffi::LLVMLinkage::ExternalWeakLinkage => ExternalWeak,
            ffi::LLVMLinkage::CommonLinkage => Common,
            ffi::LLVMLinkage::LinkerPrivateLinkage => LinkerPrivate,
            ffi::LLVMLinkage::LinkerPrivateWeakLinkage => LinkerPrivateWeak,
            ffi::LLVMLinkage::DLLImportLinkage => DLLImport,
            ffi::LLVMLinkage::DLLExportLinkage => DLLExport,
            ffi::LLVMLinkage::GhostLinkage => Ghost,
        }
    }

    pub fn set_linkage(&mut self, linkage: Linkage) {
        use Linkage::*;

        let linkage = match linkage {
            External => ffi::LLVMLinkage::ExternalLinkage,
            AvailableExternally => ffi::LLVMLinkage::AvailableExternallyLinkage,
            LinkOnceAny => ffi::LLVMLinkage::LinkOnceAnyLinkage,
            LinkOnceODR => ffi::LLVMLinkage::LinkOnceODRLinkage,
            LinkOnceODRAutoHide => ffi::LLVMLinkage::LinkOnceODRAutoHideLinkage,
            WeakAny => ffi::LLVMLinkage::WeakAnyLinkage,
            WeakODR => ffi::LLVMLinkage::WeakODRLinkage,
            Appending => ffi::LLVMLinkage::AppendingLinkage,
            Internal => ffi::LLVMLinkage::InternalLinkage,
            Private => ffi::LLVMLinkage::PrivateLinkage,
            ExternalWeak => ffi::LLVMLinkage::ExternalWeakLinkage,
            Common => ffi::LLVMLinkage::CommonLinkage,
            LinkerPrivate => ffi::LLVMLinkage::LinkerPrivateLinkage,
            LinkerPrivateWeak => ffi::LLVMLinkage::LinkerPrivateWeakLinkage,
            DLLImport => ffi::LLVMLinkage::DLLImportLinkage,
            DLLExport => ffi::LLVMLinkage::DLLExportLinkage,
            Ghost => ffi::LLVMLinkage::GhostLinkage,
        };

        unsafe { ffi::LLVMSetLinkage(self.value, linkage) }
    }

    pub fn get_alignment(&self) -> u32 {
        unsafe { ffi::LLVMGetAlignment(self.value) }
    }

    pub fn set_alignment(&mut self, alignment: u32) {
        unsafe { ffi::LLVMSetAlignment(self.value, alignment) }
    }

    pub fn get_visibility(&self) -> Visibility {
        use Visibility::*;

        match unsafe { ffi::LLVMGetVisibility(self.value) } {
            ffi::LLVMVisibility::DefaultVisibility => Default,
            ffi::LLVMVisibility::HiddenVisibility => Hidden,
            ffi::LLVMVisibility::ProtectedVisibility => Protected,
        }
    }

    pub fn set_visibility(&mut self, visibility: Visibility) {
        use Visibility::*;

        let visibility = match visibility {
            Default => ffi::LLVMVisibility::DefaultVisibility,
            Hidden => ffi::LLVMVisibility::HiddenVisibility,
            Protected => ffi::LLVMVisibility::ProtectedVisibility,
        };

        unsafe { ffi::LLVMSetVisibility(self.value, visibility) }
    }

    pub fn get_call_conv(&self) -> CallConv {
        unsafe { ffi::LLVMGetFunctionCallConv(self.value) }
    }

    pub fn set_call_conv(&mut self, call_conv: CallConv) {
        unsafe { ffi::LLVMSetFunctionCallConv(
            self.value,
            match call_conv {
                CallConv::X86_StdCall => 64,
                _ => call_conv as u32,
            }
        ) }
    }

    pub fn get_section(&self) -> Option<&str> {
        let ptr = unsafe { ffi::LLVMGetSection(self.value) };
        if ptr.is_null() {
            None
        } else {
            Some(unsafe { std::ffi::CStr::from_ptr(ptr) }.to_str().unwrap())
        }
    }

    pub fn set_section(&mut self, section: &str) {
        let cstr = std::ffi::CString::new(section).unwrap();
        unsafe { ffi::LLVMSetSection(self.value, cstr.as_ptr() as *const i8) }
    }

    pub fn get_param_count(&self) -> u32 {
        unsafe { ffi::LLVMCountParams(self.value) }
    }

    pub fn get_params(&self) -> Vec<llvm::Value> {
        let count = self.get_param_count();
        let mut params = Vec::with_capacity(count as usize);

        unsafe {
            ffi::LLVMGetParams(self.value, params.as_mut_ptr());
            params.set_len(count as usize);
        }

        params.into_iter().map(llvm::Value::new).collect()
    }

    pub fn get_param(&self, index: u32) -> Option<llvm::Value> {
        let ptr = unsafe { ffi::LLVMGetParam(self.value, index) };
        if ptr.is_null() {
            None
        } else {
            Some(llvm::Value::new(ptr))
        }
    }

    pub fn append_basic_block(&mut self, name: &str, ctx: Option<&llvm::Context>) -> llvm::BasicBlock {
        let cstr = ffi::to_cstr(name);
        let ptr = match ctx {
            Some(ctx) => unsafe { ffi::LLVMAppendBasicBlockInContext(ctx.as_ptr(), self.value, cstr) },
            None => unsafe { ffi::LLVMAppendBasicBlock(self.value, cstr) },
        };

        llvm::BasicBlock::from_ptr(ptr)
    }

    pub fn count_basic_blocks(&self) -> u32 {
        unsafe { ffi::LLVMCountBasicBlocks(self.value) }
    }

    pub fn get_basic_blocks(&self) -> Vec<llvm::BasicBlock> {
        let count = self.count_basic_blocks();
        let mut blocks = Vec::with_capacity(count as usize);

        unsafe {
            ffi::LLVMGetBasicBlocks(self.value, blocks.as_mut_ptr());
            blocks.set_len(count as usize);
        }

        blocks.into_iter().map(llvm::BasicBlock::from_ptr).collect()
    }

    pub fn get_entry_basic_block(&self) -> Option<llvm::BasicBlock> {
        let ptr = unsafe { ffi::LLVMGetEntryBasicBlock(self.value) };
        if ptr.is_null() {
            None
        } else {
            Some(llvm::BasicBlock::from_ptr(ptr))
        }
    }

    pub fn get_first_basic_block(&self) -> Option<llvm::BasicBlock> {
        let ptr = unsafe { ffi::LLVMGetFirstBasicBlock(self.value) };
        if ptr.is_null() {
            None
        } else {
            Some(llvm::BasicBlock::from_ptr(ptr))
        }
    }

    pub fn get_last_basic_block(&self) -> Option<llvm::BasicBlock> {
        let ptr = unsafe { ffi::LLVMGetLastBasicBlock(self.value) };
        if ptr.is_null() {
            None
        } else {
            Some(llvm::BasicBlock::from_ptr(ptr))
        }
    }

    pub fn verify(&self, action: VerifyAction) -> bool {
        unsafe { 
            ffi::LLVMVerifyFunction(
                self.value,
                match action {
                    VerifyAction::Return => ffi::LLVMVerifierFailureAction::ReturnStatusAction,
                    VerifyAction::Print => ffi::LLVMVerifierFailureAction::PrintMessageAction,
                    VerifyAction::Abort => ffi::LLVMVerifierFailureAction::AbortProcessAction,
                }
            )
        }
    }
}
