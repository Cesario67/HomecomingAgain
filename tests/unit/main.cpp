// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>

#include "core/Strings.h"
#include "graphics/BorderlessWindow.h"
#include "graphics/ShaderConstantTable.h"
#include "patches/FrameRateCap.h"

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
    Check(hca::EqualsIgnoreCase(L"SilentHill.exe", L"silenthill.EXE"), "comparaison insensible à la casse");
    Check(!hca::EqualsIgnoreCase(L"SilentHill.exe", L"SilentHill2.exe"), "noms différents");
    Check(hca::WideToUtf8(L"é") == "\xC3\xA9", "conversion UTF-8 d'un caractère accentué");
    const std::wstring stamp = hca::FileTimestamp();
    Check(stamp.size() == 19 && stamp[8] == L'-' && stamp[15] == L'-', "horodatage AAAAMMJJ-HHMMSS-mmm");
}

void TestBorderless() {
    std::printf("Fenêtre sans bordure\n");
    const DWORD framed = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS;
    const DWORD result = hca::graphics::BorderlessStyle(framed);
    Check((result & WS_CAPTION) == 0 && (result & WS_THICKFRAME) == 0 && (result & WS_SYSMENU) == 0, "cadre, titre et menu retirés");
    Check((result & WS_POPUP) != 0 && (result & WS_VISIBLE) != 0 && (result & WS_CLIPSIBLINGS) != 0, "WS_POPUP ajouté, visibilité conservée");
    const DWORD ex = WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_TOPMOST;
    const DWORD exResult = hca::graphics::BorderlessExStyle(ex);
    Check((exResult & (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)) == 0 && (exResult & WS_EX_TOPMOST) != 0, "bords étendus retirés, TOPMOST conservé");
    Check(!hca::graphics::ApplyBorderless(nullptr), "handle nul ignoré");
}

void TestFramePeriod() {
    std::printf("Plafond de cadence\n");
    Check(hca::patches::FramePeriodMs(60) > 16.66f && hca::patches::FramePeriodMs(60) < 16.67f, "60 i/s : 16,67 ms");
    Check(hca::patches::FramePeriodMs(30) > 33.33f && hca::patches::FramePeriodMs(30) < 33.34f, "30 i/s : 33,33 ms");
    Check(hca::patches::FramePeriodMs(0) == 0.0f && hca::patches::FramePeriodMs(5000) == 0.0f, "valeurs hors bornes refusées");
}

void TestShaderConstantTable() {
    std::printf("Table de constantes des shaders\n");
    // vs_3_0 avec un bloc CTAB : en-tête (7 mots), une constante (5 mots), nom « View » (2 mots).
    const std::uint32_t tokens[] = {
        0xFFFE0300,                              // version vs_3_0
        (15u << 16) | 0xFFFE,                    // commentaire de 15 mots
        0x42415443,                              // 'CTAB'
        28, 0, 0xFFFE0300, 1, 28, 0, 0,          // en-tête : 1 constante décrite à l'octet 28
        48, (4u << 16) | 2, 4, 0, 0,             // nom à l'octet 48, registres c4 x4
        0x77656956, 0,                           // « View »
        0x0000FFFF};                             // fin
    const auto constants = hca::graphics::ParseConstantTable(tokens, std::size(tokens));
    Check(constants.size() == 1 && constants[0].name == "View", "nom de la constante");
    Check(constants.size() == 1 && constants[0].registerSet == 2 && constants[0].registerIndex == 4 &&
              constants[0].registerCount == 4,
          "registres c4 à c7");
    const std::uint32_t noTable[] = {0xFFFE0300, 0x0000FFFF};
    Check(hca::graphics::ParseConstantTable(noTable, std::size(noTable)).empty(), "shader sans table");
    Check(hca::graphics::ParseConstantTable(tokens, 5).empty(), "bytecode tronqué refusé");
}

}  // namespace

int main() {
    TestStrings();
    TestBorderless();
    TestFramePeriod();
    TestShaderConstantTable();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
