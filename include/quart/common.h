#pragma once

#include <stdint.h>

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

#ifndef QUART_PATH
    #define QUART_PATH "lib"
#endif

#define FILE_EXTENSION ".qr"

#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_NAME "windows"
#elif defined(__APPLE__)
    #define PLATFORM_NAME "macos"
#elif defined(__ANDROID__)
    #define PLATFORM_NAME "android"
#elif defined(__linux__)
    #define PLATFORM_NAME "linux"
#else
    #define PLATFORM_NAME "unknown"
#endif

#ifdef __GNUC__
    #define UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
    #define UNREACHABLE() __assume(0)
#else
    #define UNREACHABLE() assert(false && "Unreachable")
#endif