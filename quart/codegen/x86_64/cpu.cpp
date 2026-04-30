#include <quart/codegen/x86_64/cpu.h>

namespace quart::x86_64 {

StringView to_string(ConditionCode cc) {
    switch (cc) {
        case ConditionCode::None: return "???";

        case ConditionCode::a:   return "a";
        case ConditionCode::ae:  return "ae";
        case ConditionCode::b:   return "b";
        case ConditionCode::be:  return "be";
        case ConditionCode::c:   return "c";
        case ConditionCode::g:   return "g";
        case ConditionCode::ge:  return "ge";
        case ConditionCode::l:   return "l";
        case ConditionCode::le:  return "le";
        case ConditionCode::na:  return "na";
        case ConditionCode::nae: return "nae";
        case ConditionCode::nb:  return "nb";
        case ConditionCode::nbe: return "nbe";
        case ConditionCode::nc:  return "nc";
        case ConditionCode::ng:  return "ng";
        case ConditionCode::nge: return "nge";
        case ConditionCode::nl:  return "nl";
        case ConditionCode::nle: return "nle";
        case ConditionCode::no:  return "no";
        case ConditionCode::np:  return "np";
        case ConditionCode::nz:  return "nz";
        case ConditionCode::o:   return "o";
        case ConditionCode::p:   return "p";
        case ConditionCode::pe:  return "pe";
        case ConditionCode::po:  return "po";
        case ConditionCode::s:   return "s";
        case ConditionCode::z:   return "z";
        case ConditionCode::e:   return "e";
        case ConditionCode::ne:  return "ne";
        case ConditionCode::ns:  return "ns";
    }

    return "???";
}

StringView to_string(BinaryInstruction instruction) {
    switch (instruction) {
        case BinaryInstruction::cmp: return "cmp";
        case BinaryInstruction::add: return "add";
        case BinaryInstruction::sub: return "sub";
        case BinaryInstruction::mul: return "mul";
        case BinaryInstruction::sal: return "sal";
        case BinaryInstruction::sar: return "sar";
        case BinaryInstruction::shr: return "shr";
        case BinaryInstruction::shl: return "shl";
    }

    return "???";
}

StringView to_string(DataType data_type) {
    switch (data_type) {
        case DataType::Byte:  return "BYTE";
        case DataType::Word:  return "WORD";
        case DataType::DWord: return "DWORD";
        case DataType::QWord: return "QWORD";
    }

    return "???";
}

ConditionCode negate(ConditionCode cc) {
    switch (cc) {
        case ConditionCode::None: return ConditionCode::None;

        case ConditionCode::a:   return ConditionCode::na;      
        case ConditionCode::ae:  return ConditionCode::nae;     
        case ConditionCode::b:   return ConditionCode::nb;      
        case ConditionCode::be:  return ConditionCode::nbe;
        case ConditionCode::c:   return ConditionCode::nc;      
        case ConditionCode::g:   return ConditionCode::ng;      
        case ConditionCode::ge:  return ConditionCode::nge;
        case ConditionCode::l:   return ConditionCode::nl;      
        case ConditionCode::le:  return ConditionCode::nle;
        case ConditionCode::na:  return ConditionCode::a;
        case ConditionCode::nae: return ConditionCode::ae;
        case ConditionCode::nb:  return ConditionCode::b;
        case ConditionCode::nbe: return ConditionCode::be;
        case ConditionCode::nc:  return ConditionCode::c;
        case ConditionCode::ng:  return ConditionCode::g;
        case ConditionCode::nge: return ConditionCode::ge;
        case ConditionCode::nl:  return ConditionCode::l;
        case ConditionCode::nle: return ConditionCode::le;
        case ConditionCode::no:  return ConditionCode::o;
        case ConditionCode::np:  return ConditionCode::p;
        case ConditionCode::nz:  return ConditionCode::z;
        case ConditionCode::o:   return ConditionCode::no;
        case ConditionCode::p:   return ConditionCode::np;      
        case ConditionCode::pe:  return ConditionCode::po;
        case ConditionCode::po:  return ConditionCode::pe;
        case ConditionCode::s:   return ConditionCode::ns;
        case ConditionCode::z:   return ConditionCode::nz;
        case ConditionCode::e:   return ConditionCode::ne;      
        case ConditionCode::ne:  return ConditionCode::e;
        case ConditionCode::ns:  return ConditionCode::s;
    }

    return ConditionCode::None;
}

}