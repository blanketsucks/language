#include <quart/target.h>

namespace quart {

static Target s_build_target = { {} }; // NOLINT

Target const& Target::build() {
    return s_build_target;
}

String Target::normalize(StringView triple) {
    return llvm::Triple::normalize(triple);
}

void Target::set_build_target(const Target& target) {
    s_build_target = target;
}

size_t Target::word_size() const {
    if (this->is_32bit()) {
        return 32; // NOLINT
    } else {
        return 64; // NOLINT
    }
}

}