#pragma once

#include <quart/common.h>

#include <llvm/TargetParser/Triple.h>

namespace quart {

class Target {
public:
    Target(const String& name) : m_triple(name) {}

    static String normalize(StringView);

    static Target const& build();
    static void set_build_target(const Target&);

    llvm::Triple const& triple() const { return m_triple; }

    StringView arch() const { return m_triple.getArchName(); }
    StringView os() const { return m_triple.getOSName(); }
    StringView vendor() const { return m_triple.getVendorName(); }

    bool is_32bit() const { return m_triple.isArch32Bit(); }
    bool is_64bit() const { return m_triple.isArch64Bit(); }

    size_t word_size() const;

private:
    llvm::Triple m_triple;
};


}