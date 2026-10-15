#include <quart/filesystem.h>

namespace quart::fs {

fs::Path with_extension(const fs::Path& p, const fs::Path& ext) {
    fs::Path path(p);
    path.replace_extension(ext);

    return path;
}

fs::Path with_extension(const fs::Path& p) {
    fs::Path path(p);
    path.replace_extension();

    return path;
}

}