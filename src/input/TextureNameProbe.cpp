#include "input/TextureNameProbe.h"

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <set>
#include <string>

#include "core/Config.h"
#include "core/Log.h"

namespace hca::input {

namespace {

constexpr std::size_t kMaxLogged = 120;
// Les deux appelants qui affichent une icône d'action (0x109076B0, 0x1092E4D0) sont notés quel que soit le nom.
constexpr std::uintptr_t kPromptCallers[] = {0x1090777D, 0x1092E582};

std::mutex g_lock;
std::set<std::string> g_seen;
int g_lastMode = -2;
int g_lastFlags = -2;

bool ReadTexture(const void* item, std::uintptr_t& handle, std::uintptr_t& resource) noexcept {
    __try {
        handle = *reinterpret_cast<const std::uintptr_t*>(static_cast<const std::uint8_t*>(item) + 0x80);
        resource = handle != 0 ? *reinterpret_cast<const std::uintptr_t*>(handle + 0x58) : 0;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Mode d'entrée lu par 0x109076B0, 0x10907540 et 0x10986360 avant de choisir une icône : octet +0x178 de
// [[[0x111D2460]+0x38]+8] (0 : icônes clavier), puis bit 0 du mot +0x220 de l'objet renvoyé par 0x104F7170
// ([0x11597320] si son mot +0x14 a le bit 0x8000 ; 1 : icône d'action PC). -1 si illisible ou absent.
void ReadInputMode(int& mode, int& flags) noexcept {
    mode = -1;
    flags = -1;
    __try {
        const auto root = *reinterpret_cast<const std::uintptr_t*>(0x111D2460);
        const auto inner = *reinterpret_cast<const std::uintptr_t*>(root + 0x38);
        const auto state = *reinterpret_cast<const std::uintptr_t*>(inner + 8);
        mode = *reinterpret_cast<const std::uint8_t*>(state + 0x178);
        const auto object = *reinterpret_cast<const std::uintptr_t*>(0x11597320);
        if (object != 0 && (*reinterpret_cast<const std::uint32_t*>(object + 0x14) & 0x8000) != 0) {
            flags = static_cast<int>(*reinterpret_cast<const std::uint32_t*>(object + 0x220));
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

}  // namespace

bool IsButtonIconName(const char* name) noexcept {
    if (name == nullptr) {
        return false;
    }
    return std::strstr(name, "button") != nullptr || std::strstr(name, "flash") != nullptr ||
           std::strncmp(name, "pc_", 3) == 0;
}

void OnTextureApplied(void* item, const char* requested, const char* applied, std::uintptr_t caller) noexcept {
    if (!GetConfig().iconLog || requested == nullptr) {
        return;
    }
    bool promptCaller = false;
    for (const std::uintptr_t prompt : kPromptCallers) {
        promptCaller = promptCaller || caller == prompt;
    }
    if (!IsButtonIconName(requested) && !promptCaller) {
        return;
    }
    std::uintptr_t handle = 0;
    std::uintptr_t resource = 0;
    const bool read = ReadTexture(item, handle, resource);
    int mode = 0;
    int flags = 0;
    ReadInputMode(mode, flags);
    char key[200]{};
    std::snprintf(key, sizeof(key), "%s|%s|%08zX|%d|%d|%d|%d", requested, applied != nullptr ? applied : "",
                  static_cast<std::size_t>(caller), handle != 0, resource != 0, mode, flags);
    try {
        std::lock_guard<std::mutex> lock(g_lock);
        if (g_seen.size() < kMaxLogged && g_seen.insert(key).second) {
            log::Info("Icônes de boutons : « {} »{}{} demandée par {:08X}, poignée {:08X}, texture {:08X}{}, mode {}, "
                      "indicateurs {:X}",
                      requested, applied != nullptr ? " remplacée par " : "", applied != nullptr ? applied : "", caller,
                      handle, resource, read ? "" : " (illisible)", mode, flags);
        }
    } catch (...) {
    }
}

void WatchInputMode() noexcept {
    if (!GetConfig().iconLog) {
        return;
    }
    int mode = 0;
    int flags = 0;
    ReadInputMode(mode, flags);
    if (mode == g_lastMode && flags == g_lastFlags) {
        return;
    }
    g_lastMode = mode;
    g_lastFlags = flags;
    log::Info("Icônes de boutons : mode d'entrée {}, indicateurs {:X}", mode, flags);
}

}  // namespace hca::input
