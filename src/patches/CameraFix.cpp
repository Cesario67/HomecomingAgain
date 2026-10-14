#include "patches/CameraFix.h"

#include <cstdint>
#include <cstring>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "patches/CameraTuning.h"

// Valeurs lues par les relais en assembleur ci-dessous (noms C pour l'assembleur en ligne).
extern "C" {
// 1 tant que le stick droit de la première manette est incliné (voir NoteRightStick).
volatile long g_cameraPadActive = 0;
// 1 entre l'entrée du personnage et les sorties de la courbe de visée, quand la caméra « Precision » est active.
volatile long g_cameraAimActive = 0;
alignas(16) std::uint64_t g_cameraAbsMask[2] = {0x7FFFFFFFFFFFFFFFull, 0x7FFFFFFFFFFFFFFFull};
double g_cameraPadPitchGain = 0.0;
float g_cameraPitchGain = 1.0f;
float g_cameraAimOutputX = 1.0f;
float g_cameraAimOutputY = 1.0f;
// Adresses de retour dans le moteur, juste après chaque site (voir CameraFix.h).
std::uintptr_t g_cameraYawReturn = 0x108DB3DC;
std::uintptr_t g_cameraPadPitchReturn = 0x108DB003;
std::uintptr_t g_cameraPitchReturn = 0x108DB01A;
std::uintptr_t g_cameraAimInputReturn = 0x103851E8;
std::uintptr_t g_cameraAimXReturn = 0x10711F2C;
std::uintptr_t g_cameraAimYReturn = 0x1071EA2E;
}

namespace hca::patches {

namespace {

constexpr std::uintptr_t kExpectedEngineBase = 0x10000000;

struct Site {
    std::uintptr_t address;
    std::uint8_t length;
    std::uint8_t original[8];
};

// Instructions d'origine de chaque site (voir CameraFix.h).
constexpr Site kYawSite = {0x108DB3D6, 6, {0xF3, 0x0F, 0x11, 0x44, 0x24, 0x28}};
constexpr Site kPadPitchSite = {0x108DAFFB, 8, {0xF2, 0x0F, 0x59, 0x0D, 0x90, 0x12, 0xF9, 0x10}};
constexpr Site kPitchSite = {0x108DB012, 8, {0xF3, 0x0F, 0x10, 0x0D, 0x68, 0x63, 0x1D, 0x11}};
constexpr Site kAimInputSite = {0x103851E0, 8, {0xF3, 0x0F, 0x10, 0x8E, 0xD8, 0x0A, 0x00, 0x00}};
constexpr Site kAimXSite = {0x10711F24, 8, {0xF3, 0x0F, 0x10, 0x05, 0xE4, 0xA2, 0xF8, 0x10}};
constexpr Site kAimYSite = {0x1071EA28, 6, {0xF3, 0x0F, 0x11, 0x44, 0x24, 0x1C}};

constexpr std::uintptr_t kRotateActor = 0x10145F50;
constexpr std::uint8_t kRotateActorPrologue[] = {0x55, 0x8B, 0xEC, 0x83, 0xE4, 0xF0, 0x83, 0xEC, 0x30, 0x0F, 0x57, 0xC0};
constexpr std::uintptr_t kCameraManager = 0x1158982C;

using RotateActorFn = void(__cdecl*)(void* actor, float degrees);

float g_yawRadiansPerSecond = camera::kHorizontalReference;
float g_aimInputGain = camera::kAimInputGain;

bool Matches(const Site& site) {
    return std::memcmp(reinterpret_cast<const void*>(site.address), site.original, site.length) == 0;
}

template <typename T>
T Read(std::uintptr_t address) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

void ScaleFloat(std::uint8_t* address, float gain) {
    float value;
    std::memcpy(&value, address, sizeof(value));
    value *= gain;
    std::memcpy(address, &value, sizeof(value));
}

// Nom de la caméra active, lu comme le Camera Fix : gestionnaire -> +8 objet caméras ; +0x288 taille de la pile
// (0 = caméra par défaut en +0x2B0, sinon entrées de 0x60 octets en +0x28C, la dernière en -0x5C) ; caméra -> +0x248 nom.
bool IsPrecisionCameraActive() {
    const auto manager = Read<std::uintptr_t>(kCameraManager);
    if (manager == 0) {
        return false;
    }
    const auto cameras = Read<std::uintptr_t>(manager + 8);
    if (cameras == 0) {
        return false;
    }
    const auto count = Read<std::uint32_t>(cameras + 0x288);
    std::uintptr_t active = 0;
    if (count == 0) {
        active = Read<std::uintptr_t>(cameras + 0x2B0);
    } else {
        const auto stack = Read<std::uintptr_t>(cameras + 0x28C);
        if (count > 256 || stack == 0) {
            return false;
        }
        active = Read<std::uintptr_t>(stack + count * 0x60 - 0x5C);
    }
    if (active == 0) {
        return false;
    }
    return camera::IsPrecisionCameraName(Read<const char*>(active + 0x248));
}

}  // namespace

// Appelées par les relais : jamais d'exception, et toute faute de lecture laisse le comportement du moteur.
extern "C" void __cdecl HcaCameraYaw(std::uint8_t* cameraComponent, std::uint8_t* transform,
                                     const std::uint8_t* commands, const float* elapsed, float nativeYaw,
                                     float* result) noexcept {
    *result = nativeYaw;
    if (g_cameraPadActive == 0 || cameraComponent == nullptr || transform == nullptr || commands == nullptr) {
        return;
    }
    __try {
        float stickX;
        std::memcpy(&stickX, commands + 0x20, sizeof(stickX));
        if (!camera::IsStickDeflected(stickX)) {
            return;
        }
        // Suivi automatique arrêté : c'est le personnage qui tourne, et la caméra prend son cap.
        std::memset(cameraComponent + 0x120, 0, 4);
        reinterpret_cast<RotateActorFn>(kRotateActor)(
            transform - 0x1A0, camera::TurnDegrees(stickX, *elapsed, g_yawRadiansPerSecond));
        std::memcpy(transform + 0x440, transform + 0x20, 16);
        float forwardX;
        float forwardZ;
        std::memcpy(&forwardX, transform + 0x20, sizeof(forwardX));
        std::memcpy(&forwardZ, transform + 0x28, sizeof(forwardZ));
        const float heading = camera::HeadingFromForward(forwardX, forwardZ);
        std::memcpy(cameraComponent + 0x11C, &heading, sizeof(heading));
        *result = heading;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        *result = nativeYaw;
    }
}

extern "C" void __cdecl HcaCameraAimInput(std::uint8_t* actor) noexcept {
    g_cameraAimActive = 0;
    if (g_cameraPadActive == 0 || actor == nullptr) {
        return;
    }
    __try {
        if (!IsPrecisionCameraActive()) {
            return;
        }
        g_cameraAimActive = 1;
        ScaleFloat(actor + 0xAE8, g_aimInputGain);
        ScaleFloat(actor + 0xAEC, g_aimInputGain);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        g_cameraAimActive = 0;
    }
}

namespace {

// Relais : chacun refait l'instruction remplacée puis revient juste après le site. Ceux qui appellent du C++
// sauvegardent tous les registres, x87 et SSE compris (fxsave), car le moteur garde des valeurs vivantes autour.

// 0x108DB3D6 : xmm0 = cap calculé par le moteur. Pile au moment du saut : celle du moteur (esp d'origine).
__declspec(naked) void YawRelay() {
    __asm {
        pushfd
        pushad
        sub esp, 4                  // [ebx] : résultat
        mov ebx, esp                // esp d'origine = ebx + 4 + 32 + 4
        sub esp, 512
        and esp, 0FFFFFFF0h
        fxsave [esp]
        fninit
        sub esp, 8                  // pile alignée sur 16 octets après les 6 arguments
        push ebx                    // result
        sub esp, 4
        movss dword ptr [esp], xmm0 // nativeYaw
        lea eax, [ebx + 40 + 24h]
        push eax                    // elapsed
        push dword ptr [ebp + 0Ch]  // commands
        push edi                    // transform
        push esi                    // cameraComponent
        call HcaCameraYaw
        add esp, 32
        fxrstor [esp]
        movss xmm0, dword ptr [ebx]
        mov esp, ebx
        add esp, 4
        popad
        popfd
        movss dword ptr [esp + 28h], xmm0
        jmp dword ptr [g_cameraYawReturn]
    }
}

// 0x108DAFFB : le moteur a mis Y² dans xmm1 ; on le remplace par |Y| (ebx = commandes de caméra, Y en +0x24).
__declspec(naked) void PadPitchRelay() {
    __asm {
        pushfd
        cmp dword ptr [g_cameraPadActive], 0
        je native
        cvtss2sd xmm1, dword ptr [ebx + 24h]
        andpd xmm1, xmmword ptr [g_cameraAbsMask]
        mulsd xmm1, qword ptr [g_cameraPadPitchGain]
        popfd
        jmp dword ptr [g_cameraPadPitchReturn]
    native:
        popfd
        mulsd xmm1, qword ptr ds:[10F91290h]
        jmp dword ptr [g_cameraPadPitchReturn]
    }
}

// 0x108DB012 : constante de tangage de l'autre branche, multipliée par le réglage vertical.
__declspec(naked) void PitchRelay() {
    __asm {
        movss xmm1, dword ptr ds:[111D6368h]
        pushfd
        cmp dword ptr [g_cameraPadActive], 0
        je done
        mulss xmm1, dword ptr [g_cameraPitchGain]
    done:
        popfd
        jmp dword ptr [g_cameraPitchReturn]
    }
}

// 0x103851E0 : esi = personnage.
__declspec(naked) void AimInputRelay() {
    __asm {
        pushfd
        pushad
        mov ebx, esp
        sub esp, 512
        and esp, 0FFFFFFF0h
        fxsave [esp]
        fninit
        sub esp, 12
        push esi
        call HcaCameraAimInput
        add esp, 16
        fxrstor [esp]
        mov esp, ebx
        popad
        popfd
        movss xmm1, dword ptr [esi + 0AD8h]
        jmp dword ptr [g_cameraAimInputReturn]
    }
}

// 0x10711F24 : sortie horizontale de la courbe en [esp+20h] (esp d'origine, +4 après pushfd).
__declspec(naked) void AimXRelay() {
    __asm {
        pushfd
        cmp dword ptr [g_cameraAimActive], 0
        je native
        movss xmm0, dword ptr [esp + 24h]
        mulss xmm0, dword ptr [g_cameraAimOutputX]
        movss dword ptr [esp + 24h], xmm0
    native:
        popfd
        movss xmm0, dword ptr ds:[10F8A2E4h]
        jmp dword ptr [g_cameraAimXReturn]
    }
}

// 0x1071EA28 : sortie verticale dans xmm0. Les indicateurs du cmp de 0x1071EA21 servent au saut de 0x1071EA3C.
__declspec(naked) void AimYRelay() {
    __asm {
        pushfd
        cmp dword ptr [g_cameraAimActive], 0
        je native
        mulss xmm0, dword ptr [g_cameraAimOutputY]
    native:
        popfd
        movss dword ptr [esp + 1Ch], xmm0
        jmp dword ptr [g_cameraAimYReturn]
    }
}

}  // namespace

void NoteRightStick(SHORT x, SHORT y) noexcept {
    g_cameraPadActive = camera::IsRawStickDeflected(x) || camera::IsRawStickDeflected(y) ? 1 : 0;
}

void ApplyCameraFix(HMODULE engineModule) {
    const Config& config = GetConfig();
    if (!config.cameraFix) {
        log::Info("Correctif de caméra désactivé ([Camera] CameraFix=0)");
        return;
    }
    if (reinterpret_cast<std::uintptr_t>(engineModule) != kExpectedEngineBase) {
        log::Warn("Correctif de caméra non appliqué : moteur chargé à une adresse inattendue");
        return;
    }
    const Site* const sites[] = {&kYawSite, &kPadPitchSite, &kPitchSite, &kAimInputSite, &kAimXSite, &kAimYSite};
    for (const Site* site : sites) {
        if (!Matches(*site)) {
            log::Warn("Correctif de caméra non appliqué : code inattendu en 0x{:08X} (autre version du moteur ?)",
                      site->address);
            return;
        }
    }
    if (std::memcmp(reinterpret_cast<const void*>(kRotateActor), kRotateActorPrologue, sizeof(kRotateActorPrologue)) !=
        0) {
        log::Warn("Correctif de caméra non appliqué : fonction de rotation du personnage introuvable");
        return;
    }

    const camera::CameraGains gains = camera::ComputeGains(
        {config.cameraHorizontalSpeed, config.cameraVerticalSpeed, config.cameraAimHorizontalSpeed,
         config.cameraAimVerticalSpeed});
    g_yawRadiansPerSecond = gains.yawRadiansPerSecond;
    g_cameraPadPitchGain = gains.padPitchGain;
    g_cameraPitchGain = gains.pitchGain;
    g_aimInputGain = gains.aimInputGain;
    g_cameraAimOutputX = gains.aimOutputX;
    g_cameraAimOutputY = gains.aimOutputY;

    const struct {
        const Site* site;
        const void* relay;
    } hooks[] = {
        {&kYawSite, reinterpret_cast<const void*>(&YawRelay)},
        {&kPadPitchSite, reinterpret_cast<const void*>(&PadPitchRelay)},
        {&kPitchSite, reinterpret_cast<const void*>(&PitchRelay)},
        {&kAimInputSite, reinterpret_cast<const void*>(&AimInputRelay)},
        {&kAimXSite, reinterpret_cast<const void*>(&AimXRelay)},
        {&kAimYSite, reinterpret_cast<const void*>(&AimYRelay)},
    };
    for (const auto& hook : hooks) {
        if (!hooking::WriteJump(reinterpret_cast<void*>(hook.site->address), hook.relay, hook.site->length)) {
            log::Error("Correctif de caméra : écriture impossible en 0x{:08X}, les sites suivants sont ignorés",
                       hook.site->address);
            return;
        }
    }
    log::Info("Correctif de caméra appliqué : horizontal {:.2f}, vertical {:.2f}, visée {:.2f} / {:.2f}",
              camera::ClampSpeed(config.cameraHorizontalSpeed), camera::ClampSpeed(config.cameraVerticalSpeed),
              camera::ClampSpeed(config.cameraAimHorizontalSpeed), camera::ClampSpeed(config.cameraAimVerticalSpeed));
}

}  // namespace hca::patches
