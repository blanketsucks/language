#pragma once

#include <quart/common.h>
#include <quart/compiler.h>
#include <quart/errors.h>

namespace quart {

enum class CodeGenType {
    LLVM,
    x86_64
};

class CodeGen {
public:
    CodeGen() = default;
    virtual ~CodeGen() = default;

    static OwnPtr<CodeGen> create(State&, CodeGenType type, String module);

    NO_MOVE(CodeGen)
    NO_COPY(CodeGen)

    virtual ErrorOr<void> generate(CompilerOptions const&) = 0;
};

}