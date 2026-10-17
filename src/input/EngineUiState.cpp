#include "input/EngineUiState.h"

#include <windows.h>

namespace hca::input {

int EngineUiState() noexcept {
    __try {
        const auto object = *reinterpret_cast<const std::uintptr_t*>(0x11597320);
        if (object == 0 || (*reinterpret_cast<const std::uint32_t*>(object + 0x14) & 0x8000) == 0) {
            return -1;
        }
        return static_cast<int>(*reinterpret_cast<const std::uint32_t*>(object + 0x220));
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

int EnginePadFlag() noexcept {
    __try {
        const auto root = *reinterpret_cast<const std::uintptr_t*>(0x111D2460);
        const auto inner = *reinterpret_cast<const std::uintptr_t*>(root + 0x38);
        const auto state = *reinterpret_cast<const std::uintptr_t*>(inner + 8);
        return *reinterpret_cast<const std::uint8_t*>(state + 0x178);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return -1;
    }
}

}  // namespace hca::input
