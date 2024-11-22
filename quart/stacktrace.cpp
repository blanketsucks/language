#include <quart/common.h>
#include <quart/format.h>

#include <dlfcn.h>
#include <execinfo.h>

#include <llvm/Demangle/Demangle.h>

namespace quart {

constexpr u64 STACKTRACE_SIZE = 10; 

void print_stacktrace() {
    Array<void*, STACKTRACE_SIZE> buffer;
    backtrace(buffer.data(), buffer.size());

    outln("Stacktrace:");
    for (auto& address : buffer) {
        out("  ");
        if (!address) {
            continue;
        }

        Dl_info info;
        int ret = dladdr(address, &info);

        if (!ret || !info.dli_saddr) {
            if ((u64)info.dli_fbase <= 0x2) { // NOLINT
                continue;
            }

            outln("{0}: ??? at {1}", info.dli_fname, address);
            continue;
        }

        String demangled = llvm::demangle(info.dli_sname);
        outln("{0}: {1} at {2}", info.dli_fname, demangled, address);
    }

    outln();
}

}