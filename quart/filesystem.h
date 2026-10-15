#pragma once

#include <filesystem>
#include <format>

#include <quart/common.h>

namespace quart::fs {

using Path = std::filesystem::path;

fs::Path with_extension(const fs::Path& p, const fs::Path& ext);
fs::Path with_extension(const fs::Path& p);

}

template<>
struct std::formatter<quart::fs::Path> {
    constexpr auto parse(std::format_parse_context& ctx) {
        return ctx.begin();
    }

    template<typename FormatContext>
    auto format(const quart::fs::Path& path, FormatContext& ctx) const {
        return std::format_to(ctx.out(), "{}", std::string(path));
    }
};