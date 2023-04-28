use libc;

macro_rules! _binary_operations {
    ($($name:ident),*) => {
        $(
            pub fn $name(
                builder: *const LLVMBuilder,
                lhs: *const LLVMValue,
                rhs: *const LLVMValue,
                name: *const libc::c_char
            ) -> *const LLVMValue;
        )*
    }
}

macro_rules! _unary_operations {
    ($($name:ident),*) => {
        $(
            pub fn $name(
                builder: *const LLVMBuilder,
                value: *const LLVMValue,
                name: *const libc::c_char
            ) -> *const LLVMValue;
        )*
    }
}

#[inline]
pub unsafe fn to_str(ptr: *const libc::c_char) -> &'static str {
    std::ffi::CStr::from_ptr(ptr).to_str().unwrap()
}

#[inline]
pub fn to_cstr(s: &str) -> *const libc::c_char {
    let cstr = std::ffi::CString::new(s).unwrap();
    cstr.into_raw() as *const libc::c_char
}

extern "C" {
    pub type LLVMTarget;
    pub type LLVMTargetMachine;
    pub type LLVMModule;
    pub type LLVMContext;
    pub type LLVMType;
    pub type LLVMValue;
    pub type LLVMBuilder;
    pub type LLVMBasicBlock;
    pub type LLVMMetadata;
    pub type LLVMTargetData;
    pub type LLVMExecutionEngine;
    pub type LLVMGenericValue;
    pub type LLVMPassManager;

    pub type LLVMMemoryBuffer;
    pub type LLVMBinary;
    pub type LLVMSectionIterator;
    pub type LLVMSymbolIterator;
    pub type LLVMRelocationIterator;
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMCodeGenOptLevel {
    None,
    Less,
    Default,
    Aggressive,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMRelocMode {
    Default,
    Static,
    PIC,
    DynamicNoPic,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMCodeModel {
    Default,
    JITDefault,
    Small,
    Kernel,
    Medium,
    Large,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMCodeGenFileType {
    AssemblyFile,
    ObjectFile,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[allow(non_camel_case_types)]
pub enum LLVMTypeKind {
    Void,
    Half,
    Float,
    Double,
    X86_FP80,
    FP128,
    PPC_FP128,
    Label,
    Integer,
    Function,
    Struct,
    Array,
    Pointer,
    Vector,
    Metadata,
    X86_MMX,
    Token,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMLinkage {
    ExternalLinkage,
    AvailableExternallyLinkage,
    LinkOnceAnyLinkage,
    LinkOnceODRLinkage,
    LinkOnceODRAutoHideLinkage,
    WeakAnyLinkage,
    WeakODRLinkage,
    AppendingLinkage,
    InternalLinkage,
    PrivateLinkage,
    DLLImportLinkage,
    DLLExportLinkage,
    ExternalWeakLinkage,
    GhostLinkage,
    CommonLinkage,
    LinkerPrivateLinkage,
    LinkerPrivateWeakLinkage
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMVisibility {
    DefaultVisibility,
    HiddenVisibility,
    ProtectedVisibility,
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
#[allow(non_camel_case_types)]
pub enum LLVMCallConv {
    C = 0,
    Fast = 8,
    Cold = 9,
    GHC = 10,
    HiPE = 11,
    WebKit_JS = 12,
    AnyReg = 13,
    PreserveMost = 14,
    PreserveAll = 15,
    Swift = 16,
    CXX_FAST_TLS = 17,
    Tail = 18,
    CFGuard_Check = 19,
    SwiftTail = 20,
    FirstTargetCC = 64,
    X86_FastCall = 65,
    ARM_APCS = 66,
    ARM_AAPCS = 67,
    ARM_AAPCS_VFP = 68,
    MSP430_INTR = 69,
    X86_ThisCall = 70,
    PTX_Kernel = 71,
    PTX_Device = 72,
    SPIR_FUNC = 75,
    SPIR_KERNEL = 76,
    Intel_OCL_BI = 77,
    X86_64_SysV = 78,
    Win64 = 79,
    X86_VectorCall = 80,
    HHVM = 81,
    HHVM_C = 82,
    X86_INTR = 83,
    AVR_INTR = 84,
    AVR_SIGNAL = 85,
    AVR_BUILTIN = 86,
    AMDGPU_VS = 87,
    AMDGPU_GS = 88,
    AMDGPU_PS = 89,
    AMDGPU_CS = 90,
    AMDGPU_KERNEL = 91,
    X86_RegCall = 92,
    AMDGPU_HS = 93,
    MSP430_BUILTIN = 94,
    AMDGPU_LS = 95,
    AMDGPU_ES = 96,
    AArch64_VectorCall = 97,
    AArch64_SVE_VectorCall = 98,
    WASM_EmscriptenInvoke = 99,
    AMDGPU_Gfx = 100,
    M68k_INTR = 101,

    X86_StdCall = 102, // The actual value is 64, but we need to avoid conflicts with the FirstTargetCC

    MaxID = 1023
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMOpcode {
    Ret = 1,
    Br = 2,
    Switch = 3,
    IndirectBr = 4,
    Invoke = 5,
    Unreachable = 7,
    CallBr = 67,
    FNeg = 66,
    Add = 8,
    FAdd = 9,
    Sub = 10,
    FSub = 11,
    Mul = 12,
    FMul = 13,
    UDiv = 14,
    SDiv = 15,
    FDiv = 16,
    URem = 17,
    SRem = 18,
    FRem = 19,
    Shl = 20,
    LShr = 21,
    AShr = 22,
    And = 23,
    Or = 24,
    Xor = 25,
    Alloca = 26,
    Load = 27,
    Store = 28,
    GetElementPtr = 29,
    Trunc = 30,
    ZExt = 31,
    SExt = 32,
    FPToUI = 33,
    FPToSI = 34,
    UIToFP = 35,
    SIToFP = 36,
    FPTrunc = 37,
    FPExt = 38,
    PtrToInt = 39,
    IntToPtr = 40,
    BitCast = 41,
    AddrSpaceCast = 60,
    ICmp = 42,
    FCmp = 43,
    PHI = 44,
    Call = 45,
    Select = 46,
    UserOp1 = 47,
    UserOp2 = 48,
    VAArg = 49,
    ExtractElement = 50,
    InsertElement = 51,
    ShuffleVector = 52,
    ExtractValue = 53,
    InsertValue = 54,
    Freeze = 68,
    Fence = 55,
    AtomicCmpXchg = 56,
    AtomicRMW = 57,
    Resume = 58,
    LandingPad = 59,
    CleanupRet = 61,
    CatchRet = 62,
    CatchPad = 63,
    CleanupPad = 64,
    CatchSwitch = 65
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMVerifierFailureAction {
    AbortProcessAction,
    PrintMessageAction,
    ReturnStatusAction
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMModFlagBehavior {
    Error = 1,
    Warning,
    Require,
    Override,
    Append,
    AppendUnique,
    Max
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMByteOrdering {
    LittleEndian,
    BigEndian
}

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub enum LLVMBinaryType {
    Archive,
    MachOUniversalBinary,
    COFFImportFile,
    IR,
    WinRes,
    COFF,
    ELF32L,
    ELF32B,
    ELF64L,
    ELF64B,
    MachO32L,
    MachO32B,
    MachO64L,
    MachO64B,
    Wasm,
    Offload
}

extern "C" {
    pub fn LLVMInitializeNative();
    pub fn LLVMInitializeAll();

    pub fn LLVMShutdown();

    pub fn LLVMGetFirstTarget() -> *const LLVMTarget;
    pub fn LLVMGetNextTarget(target: *const LLVMTarget) -> *const LLVMTarget;
    pub fn LLVMGetTargetName(target: *const LLVMTarget) -> *const libc::c_char;
    pub fn LLVMGetTargetDescription(target: *const LLVMTarget) -> *const libc::c_char;
    pub fn LLVMGetTargetFromName(name: *const libc::c_char) -> *const LLVMTarget;
    pub fn LLVMTargetHasJIT(target: *const LLVMTarget) -> bool;
    pub fn LLVMTargetHasAsmBackend(target: *const LLVMTarget) -> bool;
    pub fn LLVMTargetHasTargetMachine(target: *const LLVMTarget) -> bool;
    pub fn LLVMGetTargetFromTriple(
        triple: *const libc::c_char, target: *mut *const LLVMTarget, error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMGetDefaultTargetTriple() -> *const libc::c_char;
    pub fn LLVMGetHostCPUName() -> *const libc::c_char;
    pub fn LLVMGetHostCPUFeatures() -> *const libc::c_char;
    pub fn LLVMCreateTargetMachine(
        target: *const LLVMTarget,
        triple: *const libc::c_char,
        cpu: *const libc::c_char,
        features: *const libc::c_char,
        level: LLVMCodeGenOptLevel,
        reloc: LLVMRelocMode,
        code_model: LLVMCodeModel
    ) -> *const LLVMTargetMachine;

    pub fn LLVMGetTargetMachineTarget(machine: *const LLVMTargetMachine) -> *const LLVMTarget;
    pub fn LLVMGetTargetMachineTriple(machine: *const LLVMTargetMachine) -> *const libc::c_char;
    pub fn LLVMGetTargetMachineCPU(machine: *const LLVMTargetMachine) -> *const libc::c_char;
    pub fn LLVMGetTargetMachineFeatureString(machine: *const LLVMTargetMachine) -> *const libc::c_char;
    pub fn LLVMDisposeTargetMachine(machine: *const LLVMTargetMachine);
    pub fn LLVMTargetMachineEmitToFile(
        machine: *const LLVMTargetMachine,
        module: *const LLVMModule,
        filename: *const libc::c_char,
        codegen: LLVMCodeGenFileType,
        error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMCreateTargetDataLayout(machine: *const LLVMTargetMachine) -> *const LLVMTargetData;

    pub fn LLVMByteOrder(target_data: *const LLVMTargetData) -> LLVMByteOrdering;
    pub fn LLVMPointerSize(target_data: *const LLVMTargetData) -> libc::c_uint;
    pub fn LLVMPointerSizeForAS(target_data: *const LLVMTargetData, address_space: libc::c_uint) -> libc::c_uint;
    pub fn LLVMIntPtrType(target_data: *const LLVMTargetData) -> *const LLVMType;
    pub fn LLVMIntPtrTypeForAS(target_data: *const LLVMTargetData, address_space: libc::c_uint) -> *const LLVMType;
    pub fn LLVMStoreSizeOfTypeInBits(target_data: *const LLVMTargetData, ty: *const LLVMType) -> libc::c_ulonglong;
    pub fn LLVMStoreSizeOfType(target_data: *const LLVMTargetData, ty: *const LLVMType) -> libc::c_ulonglong;
    pub fn LLVMABISizeOfType(target_data: *const LLVMTargetData, ty: *const LLVMType) -> libc::c_ulonglong;
    pub fn LLVMABIAlignmentOfType(target_data: *const LLVMTargetData, ty: *const LLVMType) -> libc::c_uint;
    pub fn LLVMCallFrameAlignmentOfType(target_data: *const LLVMTargetData, ty: *const LLVMType) -> libc::c_uint;

    pub fn LLVMModuleCreateWithName(name: *const libc::c_char) -> *const LLVMModule;
    pub fn LLVMModuleCreateWithNameInContext(name: *const libc::c_char, context: *const LLVMContext) -> *const LLVMModule;
    pub fn LLVMGetModuleIdentifier(module: *const LLVMModule) -> *const libc::c_char;
    pub fn LLVMSetModuleIdentifier(module: *const LLVMModule, name: *const libc::c_char, length: libc::size_t);
    pub fn LLVMDisposeModule(module: *const LLVMModule);
    pub fn LLVMGetModuleContext(module: *const LLVMModule) -> *const LLVMContext;
    pub fn LLVMDumpModule(module: *const LLVMModule);
    pub fn LLVMSetSourceFileName(module: *const LLVMModule, name: *const libc::c_char, length: libc::size_t);
    pub fn LLVMGetSourceFileName(module: *const LLVMModule) -> *const libc::c_char;
    pub fn LLVMSetDataLayout(module: *const LLVMModule, triple: *const LLVMTargetData);
    pub fn LLVMGetDataLayout(module: *const LLVMModule) -> *const LLVMTargetData;
    pub fn LLVMSetTarget(module: *const LLVMModule, triple: *const libc::c_char);
    pub fn LLVMGetTarget(module: *const LLVMModule) -> *const libc::c_char;
    pub fn LLVMAddTypeName(module: *const LLVMModule, name: *const libc::c_char, ty: *const LLVMType);
    pub fn LLVMAddFunction(module: *const LLVMModule, name: *const libc::c_char, ty: *const LLVMType) -> *const LLVMValue;
    pub fn LLVMGetNamedFunction(module: *const LLVMModule, name: *const libc::c_char) -> *const LLVMValue;
    pub fn LLVMGetFirstFunction(module: *const LLVMModule) -> *const LLVMValue;
    pub fn LLVMGetLastFunction(module: *const LLVMModule) -> *const LLVMValue;
    pub fn LLVMGetNextFunction(func: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMAddModuleFlag(
        module: *const LLVMModule, 
        behavior: LLVMModFlagBehavior, 
        key: *const libc::c_char, 
        key_len: libc::c_uint,
        value: *const LLVMMetadata
    );
    pub fn LLVMVerifyModule(module: *const LLVMModule, action: LLVMVerifierFailureAction, error: *mut *mut libc::c_char) -> bool;

    pub fn LLVMContextCreate() -> *const LLVMContext;
    pub fn LLVMGetGlobalContext() -> *const LLVMContext;
    pub fn LLVMContextDispose(ctx: *const LLVMContext);

    pub fn LLVMGetTypeKind(ty: *const LLVMType) -> LLVMTypeKind;
    pub fn LLVMGetTypeContext(ty: *const LLVMType) -> *const LLVMContext;
    pub fn LLVMDumpType(ty: *const LLVMType);
    pub fn LLVMTypeIsSized(ty: *const LLVMType) -> bool;
    pub fn LLVMGetTypeByName(module: *const LLVMModule, name: *const libc::c_char) -> *const LLVMType;

    pub fn LLVMInt8Type() -> *const LLVMType;
    pub fn LLVMInt16Type() -> *const LLVMType;
    pub fn LLVMInt32Type() -> *const LLVMType;
    pub fn LLVMInt64Type() -> *const LLVMType;
    pub fn LLVMInt128Type() -> *const LLVMType;
    pub fn LLVMIntType(bits: libc::c_uint) -> *const LLVMType;
    pub fn LLVMInt1Type() -> *const LLVMType;
    pub fn LLVMGetIntTypeWidth(ty: *const LLVMType) -> libc::c_uint;
    pub fn LLVMVoidType() -> *const LLVMType;
    pub fn LLVMFloatType() -> *const LLVMType;
    pub fn LLVMDoubleType() -> *const LLVMType;

    pub fn LLVMPointerType(ty: *const LLVMType, address_space: libc::c_uint) -> *const LLVMType;
    pub fn LLVMGetElementType(ty: *const LLVMType) -> *const LLVMType;
    pub fn LLVMGetPointerAddressSpace(ty: *const LLVMType) -> libc::c_uint;
    
    pub fn LLVMArrayType(ty: *const LLVMType, count: libc::c_uint) -> *const LLVMType;
    pub fn LLVMGetArrayLength(ty: *const LLVMType) -> libc::c_uint;
    pub fn LLVMGetSubtypes(ty: *const LLVMType, types: *mut *const LLVMType);

    pub fn LLVMFunctionType(
        ret: *const LLVMType,
        params: *const *const LLVMType,
        param_count: libc::c_uint,
        vararg: bool
    ) -> *const LLVMType;
    pub fn LLVMIsFunctionVarArg(ty: *const LLVMType) -> bool;
    pub fn LLVMGetReturnType(ty: *const LLVMType) -> *const LLVMType;
    pub fn LLVMCountParamTypes(ty: *const LLVMType) -> libc::c_uint;
    pub fn LLVMGetParamTypes(ty: *const LLVMType, types: *mut *const LLVMType);

    pub fn LLVMStructType(
        types: *const *const LLVMType,
        count: libc::c_uint,
        packed: bool
    ) -> *const LLVMType;
    pub fn LLVMStructCreateNamed(context: *const LLVMContext, name: *const libc::c_char) -> *const LLVMType;
    pub fn LLVMStructGetName(ty: *const LLVMType) -> *const libc::c_char;
    pub fn LLVMStructSetBody(
        ty: *const LLVMType,
        types: *const *const LLVMType,
        count: libc::c_uint,
        packed: bool
    );
    pub fn LLVMCountStructElementTypes(ty: *const LLVMType) -> libc::c_uint;
    pub fn LLVMGetStructElementTypes(ty: *const LLVMType, types: *mut *const LLVMType);
    pub fn LLVMIsPackedStruct(ty: *const LLVMType) -> bool;
    pub fn LLVMStructGetTypeAtIndex(ty: *const LLVMType, index: libc::c_uint) -> *const LLVMType;

    pub fn LLVMTypeOf(value: *const LLVMValue) -> *const LLVMType;
    pub fn LLVMDumpValue(value: *const LLVMValue);
    pub fn LLVMIsConstant(value: *const LLVMValue) -> bool;
    pub fn LLVMIsUndef(value: *const LLVMValue) -> bool;
    pub fn LLVMIsNull(value: *const LLVMValue) -> bool;

    pub fn LLVMConstInt(ty: *const LLVMType, value: u64, sign_extend: bool) -> *const LLVMValue;
    pub fn LLVMConstIntOfString(ty: *const LLVMType, text: *const libc::c_char, radix: u8) -> *const LLVMValue;
    pub fn LLVMConstReal(ty: *const LLVMType, value: f64) -> *const LLVMValue;
    pub fn LLVMConstRealOfString(ty: *const LLVMType, text: *const libc::c_char) -> *const LLVMValue;
    pub fn LLVMConstIntGetZExtValue(value: *const LLVMValue) -> u64;
    pub fn LLVMConstIntGetSExtValue(value: *const LLVMValue) -> i64;
    pub fn LLVMConstRealGetDouble(value: *const LLVMValue, loses_info: *mut bool) -> f64;
    pub fn LLVMConstStringInContext(
        context: *const LLVMContext,
        text: *const libc::c_char,
        length: libc::size_t,
        dont_null_terminate: bool
    ) -> *const LLVMValue;
    pub fn LLVMConstString(
        text: *const libc::c_char,
        length: libc::size_t,
        dont_null_terminate: bool
    ) -> *const LLVMValue;
    pub fn LLVMGetAsString(value: *const LLVMValue, length: *mut libc::size_t) -> *const libc::c_char;
    pub fn LLVMGetConstStructInContext(
        context: *const LLVMContext,
        values: *const *const LLVMValue,
        count: libc::c_uint,
        packed: bool
    ) -> *const LLVMValue;
    pub fn LLVMConstStruct(
        values: *const *const LLVMValue,
        count: libc::c_uint,
        packed: bool
    ) -> *const LLVMValue;
    pub fn LLVMConstArray(ty: *const LLVMType, values: *const *const LLVMValue, count: libc::c_uint) -> *const LLVMValue;
    pub fn LLVMConstNamedStruct(ty: *const LLVMType, values: *const *const LLVMValue, count: libc::c_uint) -> *const LLVMValue;
    pub fn LLVMGetAggregateElement(value: *const LLVMValue, index: libc::c_uint) -> *const LLVMValue;
    pub fn LLVMConstNull(ty: *const LLVMType) -> *const LLVMValue;
    pub fn LLVMConstPointerNull(ty: *const LLVMType) -> *const LLVMValue;

    pub fn LLVMSetValueName2(value: *const LLVMValue, name: *const libc::c_char, length: libc::size_t);
    pub fn LLVMGetValueName2(value: *const LLVMValue, length: *mut libc::size_t) -> *const libc::c_char;

    pub fn LLVMGetInstructionOpcode(value: *const LLVMValue) -> LLVMOpcode;
    pub fn LLVMGetInstructionOpcodeName(opcode: LLVMOpcode) -> *const libc::c_char;
    pub fn LLVMGetInstructionParent(value: *const LLVMValue) -> *const LLVMBasicBlock;
    pub fn LLVMGetNextInstruction(value: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMGetPreviousInstruction(value: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMInstructionRemoveFromParent(value: *const LLVMValue);
    pub fn LLVMInstructionEraseFromParent(value: *const LLVMValue);
    pub fn LLVMDeleteInstruction(value: *const LLVMValue);

    pub fn LLVMGetNumArgOperands(value: *const LLVMValue) -> libc::c_uint;
    pub fn LLVMSetInstructionCallConv(value: *const LLVMValue, call_conv: LLVMCallConv);
    pub fn LLVMGetInstructionCallConv(value: *const LLVMValue) -> LLVMCallConv;
    pub fn LLVMSetInstrParamAlignment(value: *const LLVMValue, index: libc::c_uint, align: libc::c_uint);
    pub fn LLVMGetCalledFunctionType(value: *const LLVMValue) -> *const LLVMType;
    pub fn LLVMGetCalledValue(value: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMIsTailCall(value: *const LLVMValue) -> bool;
    pub fn LLVMSetTailCall(value: *const LLVMValue, is_tail_call: bool);

    pub fn LLVMAddIncoming(
        phi: *const LLVMValue,
        incoming_values: *const *const LLVMValue,
        incoming_blocks: *const *const LLVMBasicBlock,
        count: libc::c_uint
    );

    pub fn LLVMGetGlobalParent(global: *const LLVMValue) -> *const LLVMModule;
    pub fn LLVMIsDeclaration(global: *const LLVMValue) -> bool;
    pub fn LLVMGetLinkage(global: *const LLVMValue) -> LLVMLinkage;
    pub fn LLVMSetLinkage(global: *const LLVMValue, linkage: LLVMLinkage);
    pub fn LLVMGetSection(global: *const LLVMValue) -> *const libc::c_char;
    pub fn LLVMSetSection(global: *const LLVMValue, section: *const libc::c_char);
    pub fn LLVMGetVisibility(global: *const LLVMValue) -> LLVMVisibility;
    pub fn LLVMSetVisibility(global: *const LLVMValue, visibility: LLVMVisibility);
    pub fn LLVMGetAlignment(global: *const LLVMValue) -> libc::c_uint;
    pub fn LLVMSetAlignment(global: *const LLVMValue, alignment: libc::c_uint);
    pub fn LLVMGlobalGetValueType(global: *const LLVMValue) -> *const LLVMType;

    pub fn LLVMDeleteFunction(value: *const LLVMValue);
    pub fn LLVMHasPersonalityFn(value: *const LLVMValue) -> bool;
    pub fn LLVMSetPersonalityFn(value: *const LLVMValue, personality: *const LLVMValue);
    pub fn LLVMGetPersonalityFn(value: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMCountParams(value: *const LLVMValue) -> libc::c_uint;
    pub fn LLVMGetParams(value: *const LLVMValue, params: *mut *const LLVMValue);
    pub fn LLVMGetParam(value: *const LLVMValue, index: libc::c_uint) -> *const LLVMValue;
    pub fn LLVMGetGC(value: *const LLVMValue) -> *const libc::c_char;
    pub fn LLVMSetGC(value: *const LLVMValue, gc: *const libc::c_char);
    pub fn LLVMGetFunctionCallConv(value: *const LLVMValue) -> LLVMCallConv;
    pub fn LLVMSetFunctionCallConv(value: *const LLVMValue, cc: u32);
    pub fn LLVMVerifyFunction(value: *const LLVMValue, action: LLVMVerifierFailureAction) -> bool;

    pub fn LLVMGetOperand(value: *const LLVMValue, index: libc::c_uint) -> *const LLVMValue;
    pub fn LLVMSetOperand(value: *const LLVMValue, index: libc::c_uint, operand: *const LLVMValue);
    pub fn LLVMGetNumOperands(value: *const LLVMValue) -> libc::c_uint;

    pub fn LLVMMDStringInContext2(
        context: *const LLVMContext, str: *const libc::c_char, length: libc::size_t
    ) -> *const LLVMMetadata;
    pub fn LLVMMDNodeInContext2(
        context: *const LLVMContext, values: *const *const LLVMValue, count: libc::c_uint
    ) -> *const LLVMMetadata;
    pub fn LLVMMetadataAsValue(context: *const LLVMContext, md: *const LLVMMetadata) -> *const LLVMValue;
    pub fn LLVMValueAsMetadata(value: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMGetMDString(str: *const LLVMValue, length: *mut libc::size_t) -> *const libc::c_char;

    pub fn LLVMCreateBuilderInContext(context: *const LLVMContext) -> *const LLVMBuilder;
    pub fn LLVMCreateBuilder() -> *const LLVMBuilder;
    pub fn LLVMDisposeBuilder(builder: *const LLVMBuilder);
    pub fn LLVMGetInsertBlock(builder: *const LLVMBuilder) -> *const LLVMBasicBlock;
    pub fn LLVMPositionBuilder(
        builder: *const LLVMBuilder,
        block: *const LLVMBasicBlock,
        instruction: *const LLVMValue
    );
    pub fn LLVMPositionBuilderBefore(
        builder: *const LLVMBuilder,
        instruction: *const LLVMValue
    );
    pub fn LLVMPositionBuilderAtEnd(builder: *const LLVMBuilder, block: *const LLVMBasicBlock);

    pub fn LLVMBasicBlockAsValue(block: *const LLVMBasicBlock) -> *const LLVMValue;
    pub fn LLVMValueIsBasicBlock(value: *const LLVMValue) -> bool;
    pub fn LLVMValueAsBasicBlock(value: *const LLVMValue) -> *const LLVMBasicBlock;
    pub fn LLVMGetBasicBlockName(block: *const LLVMBasicBlock) -> *const libc::c_char;
    pub fn LLVMGetBasicBlockParent(block: *const LLVMBasicBlock) -> *const LLVMValue;
    pub fn LLVMGetBasicBlockTerminator(block: *const LLVMBasicBlock) -> *const LLVMValue;
    pub fn LLVMCountBasicBlocks(function: *const LLVMValue) -> libc::c_uint;
    pub fn LLVMGetBasicBlocks(function: *const LLVMValue, basic_blocks: *mut *const LLVMBasicBlock);
    pub fn LLVMGetFirstBasicBlock(function: *const LLVMValue) -> *const LLVMBasicBlock;
    pub fn LLVMGetLastBasicBlock(function: *const LLVMValue) -> *const LLVMBasicBlock;
    pub fn LLVMGetNextBasicBlock(block: *const LLVMBasicBlock) -> *const LLVMBasicBlock;
    pub fn LLVMGetPreviousBasicBlock(block: *const LLVMBasicBlock) -> *const LLVMBasicBlock;
    pub fn LLVMGetEntryBasicBlock(function: *const LLVMValue) -> *const LLVMBasicBlock;
    pub fn LLVMGetFirstInstruction(block: *const LLVMBasicBlock) -> *const LLVMValue;
    pub fn LLVMGetLastInstruction(block: *const LLVMBasicBlock) -> *const LLVMValue;
    pub fn LLVMCreateBasicBlockInContext(
        context: *const LLVMContext,
        name: *const libc::c_char
    ) -> *const LLVMBasicBlock;
    pub fn LLVMAppendBasicBlockInContext(
        context: *const LLVMContext,
        function: *const LLVMValue,
        name: *const libc::c_char
    ) -> *const LLVMBasicBlock;
    pub fn LLVMCreateBasicBlock(name: *const libc::c_char) -> *const LLVMBasicBlock;
    pub fn LLVMAppendBasicBlock(
        function: *const LLVMValue,
        name: *const libc::c_char
    ) -> *const LLVMBasicBlock;
    pub fn LLVMDeleteBasicBlock(block: *const LLVMBasicBlock);
    pub fn LLVMRemoveBasicBlockFromParent(block: *const LLVMBasicBlock);
 
    pub fn LLVMBuildRetVoid(builder: *const LLVMBuilder) -> *const LLVMValue;
    pub fn LLVMBuildRet(builder: *const LLVMBuilder, value: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMBuildAggregateRet(
        builder: *const LLVMBuilder, values: *const *const LLVMValue, count: libc::c_uint
    ) -> *const LLVMValue;
    pub fn LLVMBuildBr(builder: *const LLVMBuilder, dest: *const LLVMBasicBlock) -> *const LLVMValue;
    pub fn LLVMBuildCondBr(
        builder: *const LLVMBuilder,
        cond: *const LLVMValue,
        then: *const LLVMBasicBlock,
        otherwise: *const LLVMBasicBlock
    ) -> *const LLVMValue;
    pub fn LLVMBuildSwitch(
        builder: *const LLVMBuilder,
        value: *const LLVMValue,
        else_block: *const LLVMBasicBlock,
        num_cases: libc::c_uint
    ) -> *const LLVMValue;
    pub fn LLVMBuildCall(
        builder: *const LLVMBuilder,
        function: *const LLVMValue,
        args: *const *const LLVMValue,
        num_args: libc::c_uint,
        name: *const libc::c_char
    ) -> *const LLVMValue;
    pub fn LLVMBuildUnreachable(builder: *const LLVMBuilder) -> *const LLVMValue;

    _binary_operations!(
        LLVMBuildAdd, LLVMBuildNSWAdd, LLVMBuildNUWAdd, LLVMBuildFAdd,
        LLVMBuildSub, LLVMBuildNSWSub, LLVMBuildNUWSub, LLVMBuildFSub,
        LLVMBuildMul, LLVMBuildNSWMul, LLVMBuildNUWMul, LLVMBuildFMul,
        LLVMBuildUDiv, LLVMBuildSDiv, LLVMBuildExactSDiv, LLVMBuildFDiv,
        LLVMBuildURem, LLVMBuildSRem, LLVMBuildFRem,
        LLVMBuildShl, LLVMBuildLShr, LLVMBuildAShr,
        LLVMBuildAnd, LLVMBuildOr, LLVMBuildXor
    );

    _unary_operations!(LLVMBuildNeg, LLVMBuildNSWNeg, LLVMBuildNUWNeg, LLVMBuildFNeg, LLVMBuildNot);

    pub fn LLVMBuildMemSet(
        builder: *const LLVMBuilder,
        ptr: *const LLVMValue,
        value: *const LLVMValue,
        length: *const LLVMValue,
        align: libc::c_uint
    ) -> *const LLVMValue;
    pub fn LLVMBuildMemCpy(
        builder: *const LLVMBuilder,
        dest: *const LLVMValue,
        src: *const LLVMValue,
        length: *const LLVMValue,
        align: libc::c_uint
    ) -> *const LLVMValue;
    pub fn LLVMBuildMemMove(
        builder: *const LLVMBuilder,
        dest: *const LLVMValue,
        src: *const LLVMValue,
        length: *const LLVMValue,
        align: libc::c_uint
    ) -> *const LLVMValue;
    pub fn LLVMBuildAlloca(builder: *const LLVMBuilder, ty: *const LLVMType, name: *const libc::c_char) -> *const LLVMValue;
    pub fn LLVMBuildArrayAlloca(builder: *const LLVMBuilder, ty: *const LLVMType, size: *const LLVMValue, name: *const libc::c_char) -> *const LLVMValue;
    pub fn LLVMBuildMalloc(builder: *const LLVMBuilder, ty: *const LLVMType, name: *const libc::c_char) -> *const LLVMValue;
    pub fn LLVMBuildFree(builder: *const LLVMBuilder, ptr: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMBuildLoad(builder: *const LLVMBuilder, ptr: *const LLVMValue, name: *const libc::c_char) -> *const LLVMValue;
    pub fn LLVMBuildStore(builder: *const LLVMBuilder, value: *const LLVMValue, ptr: *const LLVMValue) -> *const LLVMValue;
    pub fn LLVMBuildGEP(
        builder: *const LLVMBuilder,
        ptr: *const LLVMValue,
        indices: *const *const LLVMValue,
        num_indices: libc::c_uint,
        name: *const libc::c_char
    ) -> *const LLVMValue;
    pub fn LLVMBuildInBoundsGEP(
        builder: *const LLVMBuilder,
        ptr: *const LLVMValue,
        indices: *const *const LLVMValue,
        num_indices: libc::c_uint,
        name: *const libc::c_char
    ) -> *const LLVMValue;
    pub fn LLVMBuildStructGEP(
        builder: *const LLVMBuilder,
        ptr: *const LLVMValue,
        index: libc::c_uint,
        name: *const libc::c_char
    ) -> *const LLVMValue;
    pub fn LLVMBuildGlobalString(
        builder: *const LLVMBuilder,
        str: *const libc::c_char,
        name: *const libc::c_char
    ) -> *const LLVMValue;
    pub fn LLVMBuildGlobalStringPtr(
        builder: *const LLVMBuilder,
        str: *const libc::c_char,
        name: *const libc::c_char
    ) -> *const LLVMValue;

    pub fn LLVMLinkInMCJIT();
    pub fn LLVMLinkInInterpreter();
    pub fn LLVMCreateExecutionEngineForModule(
        out: *mut *const LLVMExecutionEngine,
        module: *const LLVMModule,
        error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMCreateInterpreterForModule(
        out: *mut *const LLVMExecutionEngine,
        module: *const LLVMModule,
        error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMCreateJITCompilerForModule(
        out: *mut *const LLVMExecutionEngine,
        module: *const LLVMModule,
        opt_level: libc::c_uint,
        error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMDisposeExecutionEngine(engine: *const LLVMExecutionEngine);
    pub fn LLVMRunStaticConstructors(engine: *const LLVMExecutionEngine);
    pub fn LLVMRunStaticDestructors(engine: *const LLVMExecutionEngine);
    pub fn LLVMRunFunctionAsMain(
        engine: *const LLVMExecutionEngine,
        function: *const LLVMValue,
        argc: libc::c_uint,
        argv: *const *const libc::c_char,
        envp: *const *const libc::c_char
    ) -> libc::c_int;
    pub fn LLVMRunFunction(
        engine: *const LLVMExecutionEngine,
        function: *const LLVMValue,
        num_args: libc::c_uint,
        args: *const *const LLVMGenericValue
    ) -> *const LLVMGenericValue;
    pub fn LLVMFreeMachineCodeForFunction(engine: *const LLVMExecutionEngine, function: *const LLVMValue);
    pub fn LLVMAddModule(engine: *const LLVMExecutionEngine, module: *const LLVMModule) -> bool;
    pub fn LLVMRemoveModule(
        engine: *const LLVMExecutionEngine, 
        module: *const LLVMModule, 
        out: *mut *const LLVMModule, 
        error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMFindFunction(
        engine: *const LLVMExecutionEngine, name: *const libc::c_char, out: *mut *const LLVMValue
    ) -> bool;
    pub fn LLVMRecompileAndRelinkFunction(
        engine: *const LLVMExecutionEngine, function: *const LLVMValue
    ) -> bool;
    pub fn LLVMGetExecutionEngineTargetData(engine: *const LLVMExecutionEngine) -> *const LLVMTargetData;
    pub fn LLVMGetExecutionEngineTargetMachine(engine: *const LLVMExecutionEngine) -> *const LLVMTargetMachine;
    pub fn LLVMAddGlobalMapping(
        engine: *const LLVMExecutionEngine, global: *const LLVMValue, addr: *const libc::c_void
    ) -> bool;
    pub fn LLVMGetPointerToGlobal(
        engine: *const LLVMExecutionEngine, global: *const LLVMValue
    ) -> *const libc::c_void;
    pub fn LLVMGetGlobalValueAddress(
        engine: *const LLVMExecutionEngine, name: *const libc::c_char
    ) -> u64;
    pub fn LLVMGetFunctionAddress(
        engine: *const LLVMExecutionEngine, name: *const libc::c_char
    ) -> u64;

    pub fn LLVMCreateGenericValueOfInt(ty: *const LLVMType, n: u64, sign_extend: bool) -> *const LLVMGenericValue;
    pub fn LLVMCreateGenericValueOfPointer(ptr: *const libc::c_void) -> *const LLVMGenericValue;
    pub fn LLVMCreateGenericValueOfFloat(ty: *const LLVMType, n: f64) -> *const LLVMGenericValue;
    pub fn LLVMGenericValueIntWidth(value: *const LLVMGenericValue) -> libc::c_uint;
    pub fn LLVMGenericValueToInt(value: *const LLVMGenericValue, is_signed: bool) -> u64;
    pub fn LLVMGenericValueToPointer(value: *const LLVMGenericValue) -> *const libc::c_void;
    pub fn LLVMGenericValueToFloat(ty: *const LLVMType, value: *const LLVMGenericValue) -> f64;
    pub fn LLVMDisposeGenericValue(value: *const LLVMGenericValue);

    pub fn LLVMCreatePassManager() -> *const LLVMPassManager;
    pub fn LLVMCreateFunctionPassManagerForModule(module: *const LLVMModule) -> *const LLVMPassManager;
    pub fn LLVMRunPassManager(pass_manager: *const LLVMPassManager, module: *const LLVMModule) -> bool;
    pub fn LLVMInitializeFunctionPassManager(fpm: *const LLVMPassManager) -> bool;
    pub fn LLVMRunFunctionPassManager(fpm: *const LLVMPassManager, function: *const LLVMValue) -> bool;
    pub fn LLVMFinalizeFunctionPassManager(fpm: *const LLVMPassManager) -> bool;
    pub fn LLVMDisposePassManager(pass_manager: *const LLVMPassManager);

    pub fn LLVMCreateMemoryBufferWithContentsOfFile(
        path: *const libc::c_char, out: *mut *const LLVMMemoryBuffer, error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMCreateMemoryBufferWithSTDIN(
        out: *mut *const LLVMMemoryBuffer, error: *mut *mut libc::c_char
    ) -> bool;
    pub fn LLVMCreateMemoryBufferWithMemoryRange(
        input_data: *const libc::c_char, input_data_length: libc::size_t, name: *const libc::c_char,
        requires_null_termination: bool
    ) -> *const LLVMMemoryBuffer;
    pub fn LLVMCreateMemoryBufferWithMemoryRangeCopy(
        input_data: *const libc::c_char, input_data_length: libc::size_t, name: *const libc::c_char
    ) -> *const LLVMMemoryBuffer;
    pub fn LLVMGetBufferSize(buffer: *const LLVMMemoryBuffer) -> libc::size_t;
    pub fn LLVMGetBufferStart(buffer: *const LLVMMemoryBuffer) -> *const libc::c_char;
    pub fn LLVMDisposeMemoryBuffer(buffer: *const LLVMMemoryBuffer);

    pub fn LLVMCreateBinary(
        buffer: *const LLVMMemoryBuffer, context: *const LLVMContext, error: *mut *mut libc::c_char
    ) -> *const LLVMBinary;
    pub fn LLVMBinaryGetType(binary: *const LLVMBinary) -> LLVMBinaryType;
    pub fn LLVMDisposeSectionIterator(iterator: *const LLVMSectionIterator);
    pub fn LLVMMoveToNextSection(iterator: *const LLVMSectionIterator) -> bool;
    pub fn LLVMMoveToContainingSection(iterator: *const LLVMSectionIterator, sym: *const LLVMSymbolIterator) -> bool;
    pub fn LLVMGetSectionName(iterator: *const LLVMSectionIterator) -> *const libc::c_char;
    pub fn LLVMGetSectionContents(iterator: *const LLVMSectionIterator) -> *const libc::c_char;
    pub fn LLVMGetSectionSize(iterator: *const LLVMSectionIterator) -> u64;
    pub fn LLVMGetSectionAddress(iterator: *const LLVMSectionIterator) -> u64;
    pub fn LLVMGetSectionContainsSymbol(
        iterator: *const LLVMSectionIterator, sym: *const LLVMSymbolIterator
    ) -> bool;
    pub fn LLVMGetRelocations(iterator: *const LLVMSectionIterator) -> *const LLVMRelocationIterator;
    pub fn LLVMDisposeRelocationIterator(iterator: *const LLVMRelocationIterator);
    pub fn LLVMIsRelocationIteratorAtEnd(section: *const LLVMSectionIterator, iterator: *const LLVMRelocationIterator) -> bool;
    pub fn LLVMMoveToNextRelocation(iterator: *const LLVMRelocationIterator) -> bool;
    pub fn LLVMGetRelocationSymbol(iterator: *const LLVMRelocationIterator) -> *const LLVMSymbolIterator;
    pub fn LLVMGetRelocationType(iterator: *const LLVMRelocationIterator) -> u64;
    pub fn LLVMGetRelocationTypeName(iterator: *const LLVMRelocationIterator) -> *const libc::c_char;
    pub fn LLVMGetRelocationAddress(iterator: *const LLVMRelocationIterator) -> u64;
    pub fn LLVMGetRelocationOffset(iterator: *const LLVMRelocationIterator) -> u64;
    pub fn LLVMGetRelocationValueString(iterator: *const LLVMRelocationIterator) -> *const libc::c_char;
    pub fn LLVMObjectFileCopySymbolIterator(binary: *const LLVMBinary) -> *const LLVMSymbolIterator;
    pub fn LLVMDisposeSymbolIterator(iterator: *const LLVMSymbolIterator);
    pub fn LLVMMoveToNextSymbol(iterator: *const LLVMSymbolIterator) -> bool;
    pub fn LLVMGetSymbolName(iterator: *const LLVMSymbolIterator) -> *const libc::c_char;
    pub fn LLVMGetSymbolAddress(iterator: *const LLVMSymbolIterator) -> u64;
    pub fn LLVMGetSymbolSize(iterator: *const LLVMSymbolIterator) -> u64;
    pub fn LLVMObjectFileIsSectionIteratorAtEnd(binary: *const LLVMBinary, iterator: *const LLVMSectionIterator) -> bool;
    pub fn LLVMObjectFileIsSymbolIteratorAtEnd(binary: *const LLVMBinary, iterator: *const LLVMSymbolIterator) -> bool;
    pub fn LLVMObjectFileCopySectionIterator(binary: *const LLVMBinary) -> *const LLVMSectionIterator;
    pub fn LLVMDisposeBinary(binary: *const LLVMBinary);
}
