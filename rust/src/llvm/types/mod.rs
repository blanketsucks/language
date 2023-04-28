use crate::llvm::ffi;

pub mod array;
pub mod function;
pub mod int;
pub mod pointer;
pub mod structs;

pub use array::ArrayType;
pub use function::FunctionType;
pub use int::IntegerType;
pub use pointer::PointerType;
pub use structs::StructType;

macro_rules! _is_typeof {
    ($($n:ident => $t:ident),*) => {
        $(pub fn $n(&self) -> bool { matches!(self.kind(), TypeKind::$t) })*
    }
}

pub enum TypeKind {
    Void,
    Integer,
    Float,
    Double,
    Half,
    Function,
    Struct,
    Array,
    Pointer,
    Vector,
    Metadata,
    X86Mmx,
    Token,
    X86Fp80,
    Fp128,
    PpcFp128,
    Label
}

#[derive(Debug, Copy, Clone, PartialEq, Eq, Hash)]
pub struct Type {
    handle: *const ffi::LLVMType
}

impl Type {
    pub(crate) fn new(handle: *const ffi::LLVMType) -> Self {
        Self { handle }
    }

    pub fn void() -> Self {
        Self::new(unsafe { ffi::LLVMVoidType() })
    }

    pub fn float() -> Self {
        Self::new(unsafe { ffi::LLVMFloatType() })
    }

    pub fn double() -> Self {
        Self::new(unsafe { ffi::LLVMDoubleType() })
    }

    pub fn is_sized(&self) -> bool {
        unsafe { ffi::LLVMTypeIsSized(self.handle) }
    }

    pub fn kind(&self) -> TypeKind {
        macro_rules! _match {
            ($($name:ident => $variant:ident),*) => {
                match unsafe { ffi::LLVMGetTypeKind(self.handle) } {
                    $(ffi::LLVMTypeKind::$name => TypeKind::$variant,)*
                }
            }
        }

        _match! {
            Void => Void,
            Integer => Integer,
            Float => Float,
            Double => Double,
            Half => Half,
            Function => Function,
            Struct => Struct,
            Array => Array,
            Pointer => Pointer,
            Vector => Vector,
            Metadata => Metadata,
            X86_MMX => X86Mmx,
            Token => Token,
            X86_FP80 => X86Fp80,
            FP128 => Fp128,
            PPC_FP128 => PpcFp128,
            Label => Label
        }
    }

    _is_typeof! {
        is_void => Void, is_integer => Integer, is_float => Float, is_double => Double,
        is_half => Half, is_function => Function, is_struct => Struct, is_array => Array,
        is_pointer => Pointer, is_vector => Vector, is_metadata => Metadata,
        is_x86mmx => X86Mmx, is_token => Token, is_x86fp80 => X86Fp80, is_fp128 => Fp128,
        is_ppcfp128 => PpcFp128, is_label => Label
    }

    pub fn is_floating_point(&self) -> bool {
        matches!(self.kind(), TypeKind::Float | TypeKind::Double | TypeKind::Half |
            TypeKind::X86Fp80 | TypeKind::Fp128 | TypeKind::PpcFp128)
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMType { self.handle }

    pub fn ptr(&self) -> PointerType { 
        PointerType::new(unsafe { ffi::LLVMPointerType(self.handle, 0) })
    }

    pub fn dump(&self) {
        unsafe { ffi::LLVMDumpType(self.handle); }
        println!();
    }
}

macro_rules! impl_from {
    ($n:ident, $verify:ident, $name:literal) => {
        impl TryFrom<Type> for $n {
            type Error = String;

            fn try_from(ty: Type) -> Result<Self, Self::Error> {
                if ty.$verify() {
                    Ok(Self::new(ty.as_ptr()))
                } else {
                    Err(format!("type is not a {}", $name))
                }
            }
        }

        impl From<$n> for Type {
            fn from(ty: $n) -> Self {
                Self::new(ty.as_ptr())
            }
        }
    }
}

impl_from!(FunctionType, is_function, "function");
impl_from!(IntegerType, is_integer, "integer");
impl_from!(PointerType, is_pointer, "pointer");
impl_from!(StructType, is_struct, "struct");
impl_from!(ArrayType, is_array, "array");