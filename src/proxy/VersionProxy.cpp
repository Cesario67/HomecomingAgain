// Proxy de version.dll : c'est ce qui charge SHHFix dans le jeu.
//
// SilentHill.exe (lanceur de 33 Ko) importe VERSION.dll en premier (3 fonctions : VerQueryValueA,
// GetFileVersionInfoA, GetFileVersionInfoSizeA). version.dll n'est pas dans la liste KnownDLLs de Windows :
// une copie placée à côté de SilentHill.exe est donc chargée à la place de celle du système, avant le moteur
// (vérifié le 09/10/2026 sur l'exécutable Steam : table d'imports, ordre des descripteurs). Les 17 exports de
// la vraie version.dll sont transmis tels quels, pour rester sûr si un autre module les appelle.
//
// Équivalent C# : chaque stub est un délégué qui rappelle la même méthode d'une autre assembly, sans toucher
// aux arguments (la pile et les registres de l'appelant restent intacts).

#include <windows.h>

#include <atomic>

#include "core/Log.h"

namespace {

constexpr int kExportCount = 17;
// Ordre = index passé aux stubs ci-dessous.
constexpr const char* kExportNames[kExportCount] = {
    "GetFileVersionInfoA",       "GetFileVersionInfoByHandle", "GetFileVersionInfoExA",
    "GetFileVersionInfoExW",     "GetFileVersionInfoSizeA",    "GetFileVersionInfoSizeExA",
    "GetFileVersionInfoSizeExW", "GetFileVersionInfoSizeW",    "GetFileVersionInfoW",
    "VerFindFileA",              "VerFindFileW",               "VerInstallFileA",
    "VerInstallFileW",           "VerLanguageNameA",           "VerLanguageNameW",
    "VerQueryValueA",            "VerQueryValueW"};

std::atomic<HMODULE> g_realModule{nullptr};
std::atomic<FARPROC> g_realExports[kExportCount]{};

[[noreturn]] void FailAndExit(const wchar_t* message) noexcept {
    shhf::log::Write(shhf::log::Level::Error, "Vraie version.dll introuvable ou incomplète : arrêt du jeu");
    MessageBoxW(nullptr, message, L"SHHFix", MB_ICONERROR | MB_OK);
    TerminateProcess(GetCurrentProcess(), 1);
    for (;;) {
    }
}

HMODULE LoadRealModule() noexcept {
    HMODULE module = g_realModule.load(std::memory_order_acquire);
    if (module != nullptr) {
        return module;
    }
    // Dans un processus 32 bits, System32 est redirigé vers SysWOW64 : on obtient la version 32 bits.
    wchar_t path[MAX_PATH]{};
    const UINT length = GetSystemDirectoryW(path, MAX_PATH);
    if (length == 0 || length + 14 >= MAX_PATH) {
        FailAndExit(L"Impossible de déterminer le dossier système de Windows.");
    }
    wcscat_s(path, L"\\version.dll");
    module = LoadLibraryW(path);
    if (module == nullptr) {
        FailAndExit(L"La vraie version.dll est introuvable dans le dossier système de Windows.");
    }
    g_realModule.store(module, std::memory_order_release);
    return module;
}

}  // namespace

// Appelé par les stubs ci-dessous : résout (une fois) l'adresse de la vraie fonction.
extern "C" FARPROC __cdecl ShhfResolveVersionExport(int index) noexcept {
    FARPROC function = g_realExports[index].load(std::memory_order_acquire);
    if (function != nullptr) {
        return function;
    }
    function = GetProcAddress(LoadRealModule(), kExportNames[index]);
    if (function == nullptr) {
        FailAndExit(L"La version.dll du système n'exporte pas les fonctions attendues.");
    }
    g_realExports[index].store(function, std::memory_order_release);
    return function;
}

// Stubs « nus » : ils ne touchent pas à la pile et sautent vers la vraie fonction, qui reçoit donc exactement
// les arguments et l'adresse de retour de l'appelant. EAX, ECX et EDX sont libres pour l'appelant (stdcall).
#define SHHF_VERSION_STUB(index, name)                                  \
    extern "C" __declspec(naked) void ShhfStubVersion_##name() {        \
        __asm { push index }                                            \
        __asm { call ShhfResolveVersionExport }                         \
        __asm { add esp, 4 }                                            \
        __asm { jmp eax }                                               \
    }

SHHF_VERSION_STUB(0, GetFileVersionInfoA)
SHHF_VERSION_STUB(1, GetFileVersionInfoByHandle)
SHHF_VERSION_STUB(2, GetFileVersionInfoExA)
SHHF_VERSION_STUB(3, GetFileVersionInfoExW)
SHHF_VERSION_STUB(4, GetFileVersionInfoSizeA)
SHHF_VERSION_STUB(5, GetFileVersionInfoSizeExA)
SHHF_VERSION_STUB(6, GetFileVersionInfoSizeExW)
SHHF_VERSION_STUB(7, GetFileVersionInfoSizeW)
SHHF_VERSION_STUB(8, GetFileVersionInfoW)
SHHF_VERSION_STUB(9, VerFindFileA)
SHHF_VERSION_STUB(10, VerFindFileW)
SHHF_VERSION_STUB(11, VerInstallFileA)
SHHF_VERSION_STUB(12, VerInstallFileW)
SHHF_VERSION_STUB(13, VerLanguageNameA)
SHHF_VERSION_STUB(14, VerLanguageNameW)
SHHF_VERSION_STUB(15, VerQueryValueA)
SHHF_VERSION_STUB(16, VerQueryValueW)
