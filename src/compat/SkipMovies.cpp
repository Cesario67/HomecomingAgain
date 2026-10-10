#include "compat/SkipMovies.h"

#include <cwctype>
#include <string>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"
#include "core/Strings.h"

namespace hca::compat {

namespace {

using CreateFileAFn = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
using CreateFileWFn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

CreateFileAFn g_createFileA = nullptr;
CreateFileWFn g_createFileW = nullptr;

HANDLE Refuse(const std::wstring& path) {
    log::Info("Vidéo ignorée (SkipMovies) : {}", WideToUtf8(path));
    SetLastError(ERROR_FILE_NOT_FOUND);
    return INVALID_HANDLE_VALUE;
}

HANDLE WINAPI HookCreateFileA(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation,
                              DWORD flags, HANDLE templateFile) {
    if (name != nullptr) {
        // Les chemins du moteur sont en ASCII : conversion directe octet par octet.
        std::wstring wide;
        for (const char* p = name; *p != '\0'; ++p) {
            wide.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
        }
        if (IsMoviePath(wide)) {
            return Refuse(wide);
        }
    }
    return g_createFileA(name, access, share, security, creation, flags, templateFile);
}

HANDLE WINAPI HookCreateFileW(LPCWSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation,
                              DWORD flags, HANDLE templateFile) {
    if (name != nullptr && IsMoviePath(name)) {
        return Refuse(name);
    }
    return g_createFileW(name, access, share, security, creation, flags, templateFile);
}

}  // namespace

bool IsMoviePath(std::wstring_view path) noexcept {
    if (path.size() < 4) {
        return false;
    }
    const std::wstring_view extension = path.substr(path.size() - 4);
    return extension[0] == L'.' && std::towlower(extension[1]) == L'b' && std::towlower(extension[2]) == L'i' &&
           std::towlower(extension[3]) == L'k';
}

void InstallSkipMovies(HMODULE engineModule) {
    if (!GetConfig().skipMovies) {
        return;
    }
    const bool a = hooking::PatchImport(engineModule, "kernel32.dll", "CreateFileA", &HookCreateFileA, &g_createFileA);
    const bool w = hooking::PatchImport(engineModule, "kernel32.dll", "CreateFileW", &HookCreateFileW, &g_createFileW);
    log::Info("Vidéos du jeu ignorées (SkipMovies, option de test) : CreateFileA {}, CreateFileW {}",
              a ? "intercepté" : "non importé", w ? "intercepté" : "non importé");
}

}  // namespace hca::compat
