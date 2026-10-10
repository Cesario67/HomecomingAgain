// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <windows.h>

#include <cstdio>
#include <string>

#include "core/Strings.h"
#include "graphics/BorderlessWindow.h"

namespace {

int g_failures = 0;

void Check(bool condition, const char* name) {
    std::printf("  %s %s\n", condition ? "OK   " : "ECHEC", name);
    if (!condition) {
        ++g_failures;
    }
}

void TestStrings() {
    std::printf("Chaînes\n");
    Check(shhf::EqualsIgnoreCase(L"SilentHill.exe", L"silenthill.EXE"), "comparaison insensible à la casse");
    Check(!shhf::EqualsIgnoreCase(L"SilentHill.exe", L"SilentHill2.exe"), "noms différents");
    Check(shhf::WideToUtf8(L"é") == "\xC3\xA9", "conversion UTF-8 d'un caractère accentué");
    const std::wstring stamp = shhf::FileTimestamp();
    Check(stamp.size() == 19 && stamp[8] == L'-' && stamp[15] == L'-', "horodatage AAAAMMJJ-HHMMSS-mmm");
}

void TestBorderless() {
    std::printf("Fenêtre sans bordure\n");
    const DWORD framed = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS;
    const DWORD result = shhf::graphics::BorderlessStyle(framed);
    Check((result & WS_CAPTION) == 0 && (result & WS_THICKFRAME) == 0 && (result & WS_SYSMENU) == 0, "cadre, titre et menu retirés");
    Check((result & WS_POPUP) != 0 && (result & WS_VISIBLE) != 0 && (result & WS_CLIPSIBLINGS) != 0, "WS_POPUP ajouté, visibilité conservée");
    const DWORD ex = WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_TOPMOST;
    const DWORD exResult = shhf::graphics::BorderlessExStyle(ex);
    Check((exResult & (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)) == 0 && (exResult & WS_EX_TOPMOST) != 0, "bords étendus retirés, TOPMOST conservé");
    Check(!shhf::graphics::ApplyBorderless(nullptr), "handle nul ignoré");
}

}  // namespace

int main() {
    TestStrings();
    TestBorderless();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
