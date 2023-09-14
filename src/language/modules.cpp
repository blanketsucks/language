#include <quart/language/modules.h>
#include <quart/macros.h>

using namespace quart;

static const fs::Path FS_QUART_PATH = fs::Path(QUART_PATH);

Module::Module(
    const std::string& name, const fs::Path& path, Scope* scope
) : name(name), path(path), is_ready(false), scope(scope) {
    fs::Path parent = this->path.resolve().parent();
    this->is_standard_library = parent.is_part_of(FS_QUART_PATH);
}

std::string Module::to_string(char sep) {
    std::string str = this->path;
    if (this->is_standard_library) {
        str = this->path.remove_prefix(FS_QUART_PATH);
    }

    if (sep != '/') std::replace(str.begin(), str.end(), '/', sep);
    return str;
}
