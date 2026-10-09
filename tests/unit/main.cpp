// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <cstdio>
#include <string>

#include "core/Strings.h"

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

}  // namespace

int main() {
    TestStrings();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
