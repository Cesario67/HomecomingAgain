#include "patches/FrameRateCap.h"

#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace shhf::patches {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;
constexpr std::uintptr_t kFldInstruction = 0x10A4CC99;  // fld dword ptr ds:[0x1108E804], voir FrameRateCap.h
constexpr std::uint8_t kExpectedBytes[] = {0xD9, 0x05, 0x04, 0xE8, 0x08, 0x11};

// Lue par le limiteur du moteur à chaque image (à la place de la constante 33,32 ms).
float g_framePeriodMs = 33.32333f;

}  // namespace

float FramePeriodMs(int fps) noexcept {
    if (fps < 1 || fps > 1000) {
        return 0.0f;
    }
    return 1000.0f / static_cast<float>(fps);
}

void ApplyFrameRateCap(HMODULE engineModule) {
    const int target = GetConfig().targetFps;
    if (target == 0) {
        log::Info("Cadence du moteur laissée à 30 i/s (TargetFps=0)");
        return;
    }
    if (reinterpret_cast<std::uintptr_t>(engineModule) != kExpectedEngineBase) {
        log::Warn("Plafond de cadence non modifié : moteur chargé à une adresse inattendue");
        return;
    }
    auto* const instruction = reinterpret_cast<std::uint8_t*>(kFldInstruction);
    if (std::memcmp(instruction, kExpectedBytes, sizeof(kExpectedBytes)) != 0) {
        log::Warn("Plafond de cadence non modifié : version du moteur différente de celle analysée");
        return;
    }
    g_framePeriodMs = FramePeriodMs(target);
    // L'opérande (adresse absolue du flottant) suit les 2 octets d'opcode D9 05.
    const auto operand = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(&g_framePeriodMs));
    if (!hooking::WriteCode(instruction + 2, &operand, sizeof(operand))) {
        log::Error("Plafond de cadence : écriture impossible dans le code du moteur");
        return;
    }
    log::Info("Plafond de cadence du moteur : {} i/s (durée d'image {:.3f} ms au lieu de 33,323 ms)", target,
              g_framePeriodMs);
}

}  // namespace shhf::patches
