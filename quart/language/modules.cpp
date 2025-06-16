#include <quart/language/modules.h>
#include <quart/language/scopes.h>

namespace quart {

static const fs::Path FS_QUART_PATH = fs::Path(QUART_PATH);

Module::Module(
    String name, String qualified_name, fs::Path path, RefPtr<Scope> scope, RefPtr<Module> parent
) : Symbol(move(name), Symbol::Module, false), m_qualified_name(move(qualified_name)), m_path(move(path)), m_scope(move(scope)), m_parent(move(parent)) {
    m_scope->set_module(this);
}

}