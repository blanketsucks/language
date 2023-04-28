use crate::llvm;

use llvm::ffi;

pub type BinaryType = ffi::LLVMBinaryType;

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Binary {
    binary: *const ffi::LLVMBinary,
}

impl Binary {
    pub fn new(binary: *const ffi::LLVMBinary) -> Self {
        Self { binary }
    }

    pub fn from_memory_buffer(buffer: llvm::MemoryBuffer, context: &llvm::Context) -> Result<Self, String> {
        let mut err = std::ptr::null_mut();
        let binary = unsafe { ffi::LLVMCreateBinary(
            buffer.as_ptr(), context.as_ptr(), &mut err
        ) };

        if binary.is_null() {
            Err(unsafe { ffi::to_str(err) }.to_string())
        } else {
            Ok(Self::new(binary))
        }
    }

    pub fn as_ptr(&self) -> *const ffi::LLVMBinary { self.binary }

    pub fn get_type(&self) -> llvm::BinaryType {
        unsafe { ffi::LLVMBinaryGetType(self.binary) }
    }

    pub fn sections(&self) -> SectionIterator {
        SectionIterator::new(self)
    }

    pub fn symbols(&self) -> SymbolIterator {
        SymbolIterator::new(self)
    }
}

impl Drop for Binary {
    fn drop(&mut self) {
        unsafe { ffi::LLVMDisposeBinary(self.binary) }
    }
}

pub struct SectionIterator {
    binary: *const ffi::LLVMBinary,
    iterator: *const ffi::LLVMSectionIterator
}

impl SectionIterator {
    pub fn new(bin: &Binary) -> Self {
        Self {
            binary: bin.binary,
            iterator: unsafe { ffi::LLVMObjectFileCopySectionIterator(bin.binary) }
        }
    }
}

impl Iterator for SectionIterator {
    type Item = Section;

    fn next(&mut self) -> Option<Self::Item> {
        unsafe { ffi::LLVMMoveToNextSection(self.iterator) };
        if unsafe { ffi::LLVMObjectFileIsSectionIteratorAtEnd(self.binary, self.iterator) } {
            None
        } else {
            Some(Section::new(self.binary, self.iterator))
        }
    }
}

impl Drop for SectionIterator {
    fn drop(&mut self) {
        unsafe { ffi::LLVMDisposeSectionIterator(self.iterator) }
    }
}

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct Section {
    binary: *const ffi::LLVMBinary,
    section: *const ffi::LLVMSectionIterator,
}

impl Section {
    pub fn new(binary: *const ffi::LLVMBinary, section: *const ffi::LLVMSectionIterator) -> Self {
        Self { binary, section }
    }

    pub fn name(&self) -> &str {
        unsafe { ffi::to_str(ffi::LLVMGetSectionName(self.section)) }
    }

    pub fn contents(&self) -> &[u8] {
        let ptr = unsafe { ffi::LLVMGetSectionContents(self.section) };
        unsafe { std::slice::from_raw_parts(ptr as *const u8, self.size()) }
    }

    pub fn size(&self) -> usize {
        unsafe { ffi::LLVMGetSectionSize(self.section) as usize }
    }

    pub fn address(&self) -> u64 {
        unsafe { ffi::LLVMGetSectionAddress(self.section) }
    }

    pub fn relocations(&self) -> RelocationIterator {
        RelocationIterator::new(self.section)
    }
}

impl std::fmt::Debug for Section {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("Section")
            .field("name", &self.name())
            .field("size", &self.size())
            .field("address", &self.address())
            .finish()
    }
}

pub struct RelocationIterator {
    section: *const ffi::LLVMSectionIterator,
    iterator: *const ffi::LLVMRelocationIterator
}

impl RelocationIterator {
    pub fn new(section: *const ffi::LLVMSectionIterator) -> Self {
        Self {
            section,
            iterator: unsafe { ffi::LLVMGetRelocations(section) }
        }
    }
}

impl Iterator for RelocationIterator {
    type Item = Relocation;

    fn next(&mut self) -> Option<Self::Item> {
        unsafe { ffi::LLVMMoveToNextRelocation(self.iterator) };
        if unsafe { ffi::LLVMIsRelocationIteratorAtEnd(self.section, self.iterator) } {
            None
        } else {
            Some(Relocation::new(self.iterator))
        }
    }
}

impl Drop for RelocationIterator {
    fn drop(&mut self) {
        unsafe { ffi::LLVMDisposeRelocationIterator(self.iterator) }
    }
}

#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Relocation {
    relocation: *const ffi::LLVMRelocationIterator,
}

impl Relocation {
    pub fn new(relocation: *const ffi::LLVMRelocationIterator) -> Self {
        Self { relocation }
    }

    pub fn offset(&self) -> u64 {
        unsafe { ffi::LLVMGetRelocationOffset(self.relocation) }
    }

    pub fn symbol(&self) -> Symbol {
        Symbol::new(unsafe { ffi::LLVMGetRelocationSymbol(self.relocation) })
    }

    pub fn get_type(&self) -> u64 {
        unsafe { ffi::LLVMGetRelocationType(self.relocation) }
    }

    pub fn get_type_name(&self) -> &str {
        unsafe { ffi::to_str(ffi::LLVMGetRelocationTypeName(self.relocation)) }
    }

    pub fn value(&self) -> &str {
        unsafe { ffi::to_str(ffi::LLVMGetRelocationValueString(self.relocation)) }
    }
}

pub struct SymbolIterator {
    binary: *const ffi::LLVMBinary,
    iterator: *const ffi::LLVMSymbolIterator
}

impl SymbolIterator {
    pub fn new(bin: &Binary) -> Self {
        Self {
            binary: bin.binary,
            iterator: unsafe { ffi::LLVMObjectFileCopySymbolIterator(bin.binary) }
        }
    }
}

impl Iterator for SymbolIterator {
    type Item = Symbol;

    fn next(&mut self) -> Option<Self::Item> {
        unsafe { ffi::LLVMMoveToNextSymbol(self.iterator) };
        if unsafe { ffi::LLVMObjectFileIsSymbolIteratorAtEnd(self.binary, self.iterator) } {
            None
        } else {
            Some(Symbol::new(self.iterator))
        }
    }
}

impl Drop for SymbolIterator {
    fn drop(&mut self) {
        unsafe { ffi::LLVMDisposeSymbolIterator(self.iterator) };
    }
}

#[derive(Clone, PartialEq, Eq, Hash)]
pub struct Symbol {
    symbol: *const ffi::LLVMSymbolIterator
}

impl Symbol {
    pub fn new(symbol: *const ffi::LLVMSymbolIterator) -> Self {
        Self { symbol }
    }

    pub fn name(&self) -> &str {
        unsafe { ffi::to_str(ffi::LLVMGetSymbolName(self.symbol)) }
    }

    pub fn address(&self) -> u64 {
        unsafe { ffi::LLVMGetSymbolAddress(self.symbol) }
    }

    pub fn size(&self) -> u64 {
        unsafe { ffi::LLVMGetSymbolSize(self.symbol) }
    }
}

impl std::fmt::Debug for Symbol {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        f.debug_struct("Symbol")
            .field("name", &self.name())
            .field("address", &self.address())
            .field("size", &self.size())
            .finish()
    }
}
