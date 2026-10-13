#include "Arguments.h"

#include <cwchar>

namespace hca::dlss {

namespace {

bool ParseNumber(const std::wstring& text, std::uint64_t& value) {
    if (text.empty()) {
        return false;
    }
    wchar_t* end = nullptr;
    const bool hex = text.size() > 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X');
    value = std::wcstoull(text.c_str(), &end, hex ? 16 : 10);
    return end != nullptr && *end == L'\0';
}

}  // namespace

std::optional<Arguments> ParseArguments(const std::vector<std::wstring>& args) {
    Arguments result;
    bool seen[11]{};
    for (std::size_t i = 0; i + 1 < args.size(); i += 2) {
        const std::wstring& key = args[i];
        const std::wstring& text = args[i + 1];
        std::uint64_t n = 0;
        auto number = [&](int slot) {
            if (!ParseNumber(text, n)) {
                return false;
            }
            seen[slot] = true;
            return true;
        };
        if (key == L"--parent" && number(0)) {
            result.parentPid = static_cast<std::uint32_t>(n);
        } else if (key == L"--width" && number(1)) {
            result.width = static_cast<std::uint32_t>(n);
        } else if (key == L"--height" && number(2)) {
            result.height = static_cast<std::uint32_t>(n);
        } else if (key == L"--color" && number(3)) {
            result.color.handle = n;
        } else if (key == L"--color-format" && number(4)) {
            result.color.format = static_cast<std::uint32_t>(n);
        } else if (key == L"--color-size" && number(5)) {
            result.color.allocationSize = n;
        } else if (key == L"--depth" && number(6)) {
            result.depth.handle = n;
        } else if (key == L"--depth-format" && number(7)) {
            result.depth.format = static_cast<std::uint32_t>(n);
        } else if (key == L"--depth-size" && number(8)) {
            result.depth.allocationSize = n;
        } else if (key == L"--semaphore" && number(9)) {
            result.semaphore = n;
        } else if (key == L"--vendor" && ParseNumber(text, n)) {
            result.vendorId = static_cast<std::uint32_t>(n);
        } else if (key == L"--device" && ParseNumber(text, n)) {
            result.deviceId = static_cast<std::uint32_t>(n);
            seen[10] = true;
        } else if (key == L"--log") {
            result.logPath = text;
        } else if (key == L"--out") {
            result.outputDir = text;
        } else {
            return std::nullopt;
        }
    }
    for (bool present : seen) {
        if (!present) {
            return std::nullopt;
        }
    }
    if (result.width == 0 || result.height == 0 || result.color.handle == 0 || result.depth.handle == 0) {
        return std::nullopt;
    }
    return result;
}

}  // namespace hca::dlss
