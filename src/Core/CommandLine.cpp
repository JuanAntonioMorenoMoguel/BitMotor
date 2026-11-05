#include "Core/CommandLine.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// shellapi.h needs windows.h first (separate block so clang-format does not reorder them).
#include <shellapi.h>
#else
#include <fstream>
#include <iterator>
#endif

namespace BitMotor::Detail {

std::vector<std::string> CommandLine() {
    std::vector<std::string> args;
#ifdef _WIN32
    int count = 0;
    LPWSTR* wide = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!wide) return args;
    for (int i = 1; i < count; ++i) {
        int bytes = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
        std::string arg(bytes > 0 ? size_t(bytes - 1) : 0, '\0');
        if (bytes > 1) WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, arg.data(), bytes, nullptr, nullptr);
        args.push_back(std::move(arg));
    }
    LocalFree(wide);
#else
    std::ifstream file("/proc/self/cmdline", std::ios::binary);
    std::string all((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t start = 0;
    bool first = true;
    while (start < all.size()) {
        size_t end = all.find('\0', start);
        if (end == std::string::npos) end = all.size();
        if (!first) args.push_back(all.substr(start, end - start));
        first = false;
        start = end + 1;
    }
#endif
    return args;
}

} // namespace BitMotor::Detail
