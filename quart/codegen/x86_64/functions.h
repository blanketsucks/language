#pragma once

#include <quart/common.h>
#include <quart/format.h>

namespace quart::x86_64 {

class StringBuffer {
public:
    StringBuffer() = default;

    String const& value() const { return m_buffer; }

    void write(StringView str) { m_buffer.append(str); }
    void writeln(StringView line) { m_buffer.append(line); m_buffer.push_back('\n'); }

    template<typename ...Args>
    void fwriteln(std::format_string<Args...> fmt, Args... args) {
        String str = std::format(fmt, std::forward<Args>(args)...);
        writeln(str);
    }

private:
    String m_buffer;
};

class CodeGenFunction {
public:
    struct Local {
        size_t size = 0;
        size_t offset = 0;
    };

    static RefPtr<CodeGenFunction> create(Vector<Local> locals) {
        return RefPtr<CodeGenFunction>(new CodeGenFunction(move(locals)));
    }

    String const& code() const { return m_code; }

    Vector<Local> const& locals() const { return m_locals; }
    Optional<Local> local(size_t index) {
        if (index > m_locals.size()) {
            return {};
        }

        return m_locals[index];
    }

    void add_local(Local local) { m_locals.push_back(local); }

    void write(StringView code) { m_code.append(code); }
    void writeln(StringView line) { m_code.append(line); m_code.push_back('\n'); }

    template<typename ...Args>
    void fwriteln(std::format_string<Args...> fmt, Args... args) {
        String str = std::format(fmt, std::forward<Args>(args)...);
        writeln(str);
    }

private:
    CodeGenFunction(Vector<Local> locals) : m_locals(move(locals)) {}

    Vector<Local> m_locals;

    String m_prologue;
    String m_code;
    String m_epilogue;
};

}