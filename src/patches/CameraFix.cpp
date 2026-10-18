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

// Mode orbite ([Camera] Mode=Orbit) : cap propre de la caméra de navigation, indépendant du personnage.
bool g_orbitMode = false;
float g_orbitYaw = 0.0f;
unsigned long long g_orbitLastMs = 0;
// Personnage suivi par la caméra de navigation (transformation - 0x1A0, comme pour la fonction de rotation).
std::uintptr_t g_orbitActor = 0;
// Déplacement par rapport à la caméra : cap voulu, calculé par l'entrée du personnage et appliqué par le hook du cap
// (qui connaît la durée de l'image) ; remis à false une fois appliqué.
bool g_moveActive = false;
float g_moveHeading = 0.0f;
camera::OrbitMove g_orbitMove = camera::OrbitMove::Camera;

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

// L'entrée appartient-elle au personnage suivi par la caméra ? Propriétaire = [entrée+0x28] (fonction 0x10384E70,
// vérifié) ; qu'il soit le même objet que transformation - 0x1A0 a été vérifié par un journal temporaire en jeu le
// 18/10/2026 (toujours vrai pour le personnage joué).
bool IsOrbitActorInput(const std::uint8_t* input) {
    return g_orbitActor != 0 && Read<std::uintptr_t>(reinterpret_cast<std::uintptr_t>(input) + 0x28) == g_orbitActor;
}

float ActorHeading(const std::uint8_t* transform) {
    float forwardX;
    float forwardZ;
    std::memcpy(&forwardX, transform + 0x20, sizeof(forwardX));
    std::memcpy(&forwardZ, transform + 0x28, sizeof(forwardZ));
    return camera::HeadingFromForward(forwardX, forwardZ);
}

// Mode orbite : le stick tourne le cap de la caméra seul ; le moteur ne le ramène plus derrière le personnage (vitesse
// de suivi +0x120 remise à 0, cap calculé par le moteur ignoré). Après une interruption (autre caméra, cinématique),
// l'orbite repart du cap du moteur.
float OrbitStep(std::uint8_t* cameraComponent, const std::uint8_t* commands, float elapsed, float nativeYaw) {
    const unsigned long long now = GetTickCount64();
    // Seule une vraie interruption remet l'orbite derrière le personnage. Un changement de composant caméra n'en est
    // pas une : le 18/10/2026, après le chargement d'une sauvegarde, le cap de la caméra restait égal à celui du
    // personnage (orbite remise à zéro à chaque image, hypothèse : deux composants mis à jour tour à tour).
    if (camera::OrbitNeedsReset(g_orbitLastMs, now)) {
        g_orbitYaw = camera::WrapHeading(nativeYaw);
    }
    g_orbitLastMs = now;
    float stickX = 0.0f;
    if (g_cameraPadActive != 0) {
        std::memcpy(&stickX, commands + 0x20, sizeof(stickX));
    }
    g_orbitYaw = camera::OrbitYaw(g_orbitYaw, stickX, elapsed, g_yawRadiansPerSecond);
    std::memset(cameraComponent + 0x120, 0, 4);
    return g_orbitYaw;
}

// Entrée du personnage suivi, mode orbite hors visée : le stick gauche (+0xAD8 avant, +0xADC côté) est lu comme une
// direction à l'écran. Le cap voulu est gardé pour le hook du cap, et le moteur ne reçoit que la marche avant, réduite
// tant que le personnage ne regarde pas encore cette direction.
void ConvertMoveInput(std::uint8_t* input) {
    g_moveActive = false;
    float forward;
    float side;
    std::memcpy(&forward, input + 0xAD8, sizeof(forward));
    std::memcpy(&side, input + 0xADC, sizeof(side));
    const float magnitude = camera::StickMagnitude(forward, side);
    if (magnitude <= camera::kStickDeadzone || camera::OrbitNeedsReset(g_orbitLastMs, GetTickCount64())) {
        return;
    }
    g_moveActive = true;
    if (g_orbitMove == camera::OrbitMove::Camera) {
        // Le personnage se tourne vers la caméra ; le stick garde son effet d'origine, par rapport à lui.
        g_moveHeading = g_orbitYaw;
        return;
    }
    g_moveActive = false;
    const auto* transform = reinterpret_cast<const std::uint8_t*>(g_orbitActor + 0x1A0);
    g_moveHeading = camera::CameraRelativeHeading(forward, side, g_orbitYaw);
    const float error = camera::SignedAngle(g_moveHeading - ActorHeading(transform));
    const float walk = camera::ForwardAfterTurn(magnitude, error);
    const float none = 0.0f;
    std::memcpy(input + 0xAD8, &walk, sizeof(walk));
    std::memcpy(input + 0xADC, &none, sizeof(none));
    g_moveActive = true;
}

// Hook du cap, mode orbite : tourne le personnage vers le cap voulu, au plus kTurnRadiansPerSecond. La fonction de
// rotation augmente le cap pour un angle positif : déduit du mode Follow, où -X degrés tournent le personnage du même
// côté que l'orbite (cap - X), sens validé en jeu dans les deux modes.
void TurnTowardMove(std::uint8_t* transform, float elapsed) {
    if (!g_moveActive) {
        return;
    }
    g_moveActive = false;
    const float error = camera::SignedAngle(g_moveHeading - ActorHeading(transform));
    const float step = camera::TurnStep(error, camera::kTurnRadiansPerSecond * elapsed);
    if (step == 0.0f) {
        return;
    }
    reinterpret_cast<RotateActorFn>(kRotateActor)(transform - 0x1A0, step * 57.29578f);
    std::memcpy(transform + 0x440, transform + 0x20, 16);
}

}  // namespace

// Appelées par les relais : jamais d'exception, et toute faute de lecture laisse le comportement du moteur.
extern "C" void __cdecl HcaCameraYaw(std::uint8_t* cameraComponent, std::uint8_t* transform,
                                     const std::uint8_t* commands, const float* elapsed, float nativeYaw,
                                     float* result) noexcept {
    *result = nativeYaw;
    if (cameraComponent == nullptr || transform == nullptr || commands == nullptr) {
        return;
    }
    if (g_orbitMode) {
        __try {
            *result = OrbitStep(cameraComponent, commands, *elapsed, nativeYaw);
            g_orbitActor = reinterpret_cast<std::uintptr_t>(transform) - 0x1A0;
            TurnTowardMove(transform, *elapsed);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            *result = nativeYaw;
        }
        return;
    }
    if (g_cameraPadActive == 0) {
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
    if (actor == nullptr) {
        return;
    }
    __try {
        const bool precision = IsPrecisionCameraActive();
        // Mode orbite, hors visée : le moteur tourne aussi le personnage avec le stick droit, recopié ici en +0xAE8
        // (relevé du 18/10/2026 : son cap suivait celui de la caméra). Mis à 0 pour le seul personnage suivi par la
        // caméra ; les autres personnages et la visée gardent leur entrée.
        if (g_orbitMode && !precision && IsOrbitActorInput(actor)) {
            std::memset(actor + 0xAE8, 0, 4);
            ConvertMoveInput(actor);
        } else {
            g_moveActive = false;
        }
        if (g_cameraPadActive == 0 || !precision) {
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
    g_orbitMode = config.cameraMode == camera::CameraMode::Orbit;
    g_orbitMove = config.cameraOrbitMove;

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
    log::Info("Correctif de caméra appliqué (mode {}) : horizontal {:.2f}, vertical {:.2f}, visée {:.2f} / {:.2f}",
              !g_orbitMode ? "suivi" : g_orbitMove == camera::OrbitMove::Stick ? "orbite, marche vers le stick" : "orbite, marche vers la caméra", camera::ClampSpeed(config.cameraHorizontalSpeed), camera::ClampSpeed(config.cameraVerticalSpeed),
              camera::ClampSpeed(config.cameraAimHorizontalSpeed), camera::ClampSpeed(config.cameraAimVerticalSpeed));
}

}  // namespace hca::patches
