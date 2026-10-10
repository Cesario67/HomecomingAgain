// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>

#include "compat/MovieSkip.h"
#include "compat/SkipMovies.h"
#include "core/Strings.h"
#include "graphics/BorderlessWindow.h"
#include "graphics/FilmGrain.h"
#include "graphics/Projection.h"
#include "graphics/ShaderConstantTable.h"
#include "graphics/ShadowResolution.h"
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

void TestShadowResolution() {
    namespace shadows = hca::graphics::shadows;
    std::printf("Ombres\n");
    Check(shadows::AtlasSizeFor(4096) == 4096 && shadows::AtlasSizeFor(2048) == 2048 && shadows::AtlasSizeFor(3000) == 2048,
          "tailles d'atlas acceptées");
    Check(shadows::IsAtlasTexture(2048, 2048, D3DUSAGE_RENDERTARGET, D3DFMT_R32F), "atlas reconnu");
    Check(!shadows::IsAtlasTexture(1920, 1080, D3DUSAGE_RENDERTARGET, D3DFMT_R32F) &&
              !shadows::IsAtlasTexture(2048, 2048, 0, D3DFMT_R32F),
          "profondeur de l'écran et texture ordinaire ignorées");
    const D3DVIEWPORT9 tile{1024, 0, 1024, 1024, 0.0f, 1.0f};
    const D3DVIEWPORT9 scaled = shadows::ScaleViewport(tile, 4096);
    Check(scaled.X == 2048 && scaled.Y == 0 && scaled.Width == 2048 && scaled.Height == 2048, "carré de l'atlas agrandi");
    float constants[8] = {1, 1, 1.0f / 2048, 1.0f / 2048, 1, 1, 0.5f, 0.5f};
    Check(shadows::ScaleFilterConstants(46, constants, 2, 4096) && constants[2] == 1.0f / 4096 && constants[6] == 0.5f,
          "décalage du filtre ramené à un texel, autres valeurs intactes");
    float outside[4] = {1, 1, 1.0f / 2048, 1.0f / 2048};
    Check(!shadows::ScaleFilterConstants(80, outside, 1, 4096), "registres hors du filtre ignorés");
}

void TestProjection() {
    std::printf("Projection de la caméra\n");
    const float relevee[16] = {1.6407f, 0, 0, 0, 0, 2.9167f, 0, 0, 0, 0, 1.0002f, -3.0006f, 0, 0, 1, 0};
    hca::graphics::Projection projection;
    Check(hca::graphics::ParseProjection(relevee, projection) && projection.yScale == 2.9167f && projection.b == -3.0006f,
          "projection du relevé reconnue");
    const float worldViewProj[16] = {0.45928f, 1e-8f, -1.5751f, 787.56f, 0.25f, 2.9f, 0.07f, -316.4f,
                                     0.95f, -0.09f, 0.28f, -642.1f, 0.95f, -0.09f, 0.28f, -639.0f};
    Check(!hca::graphics::ParseProjection(worldViewProj, projection), "matrice d'objet refusée");
}

void TestFilmGrain() {
    namespace grain = hca::graphics::grain;
    std::printf("Grain de pellicule\n");
    float values[8] = {0.00078125f, 0.00078125f, 1.0f, 0.0f, 0.45f, 0.75f, 0.0005f, 0.0009f};
    Check(grain::ScaleIntensity(14, values, 2, 0.5f) && values[2] == 0.5f && values[0] == 0.00078125f && values[6] == 0.0005f,
          "seule l'intensité (c14.z) est réduite");
    float other[4] = {1, 1, 1, 1};
    Check(!grain::ScaleIntensity(15, other, 1, 0.0f) && other[2] == 1.0f, "autres registres intacts");
    // Table de constantes avec une seule variable « grainTexture » (s2).
    const std::uint32_t withGrain[] = {
        0xFFFF0300, (18u << 16) | 0xFFFE, 0x42415443,
        28, 0, 0xFFFF0300, 1, 28, 0, 0,
        48, (2u << 16) | 3, 1, 0, 0,
        0x69617267, 0x7865546E, 0x65727574, 0, 0, 0,
        0x0000FFFF};
    Check(grain::IsGrainShader(reinterpret_cast<const DWORD*>(withGrain)), "shader du grain reconnu");
    const std::uint32_t noTable[] = {0xFFFF0300, 0x0000FFFF};
    Check(!grain::IsGrainShader(reinterpret_cast<const DWORD*>(noTable)), "autre shader ignoré");
}

void TestSkipMovies() {
    std::printf("Vidéos ignorées\n");
    Check(hca::compat::IsMoviePath(L"..\\Engine\\movies\\New_GURNEY_RIDE_FINAL.bik") && hca::compat::IsMoviePath(L"X.BIK"),
          "vidéos Bink reconnues, casse ignorée");
    Check(!hca::compat::IsMoviePath(L"vars_pc.cfg") && !hca::compat::IsMoviePath(L"bik") && !hca::compat::IsMoviePath(L"a.bikx"),
          "autres fichiers ouverts normalement");
}

void TestMovieSkip() {
    std::printf("Passer les cinématiques\n");
    std::uint32_t target = 0;
    Check(hca::compat::ShouldSkipTo({1280, 720, 900, 120}, true, target) && target == 900, "saut à la dernière image");
    Check(!hca::compat::ShouldSkipTo({1280, 720, 900, 120}, false, target), "rien sans la touche");
    Check(!hca::compat::ShouldSkipTo({1280, 720, 900, 900}, true, target) && !hca::compat::ShouldSkipTo({0, 0, 1, 1}, true, target),
          "vidéo déjà finie ou vide ignorée");
}

}  // namespace

int main() {
    TestStrings();
    TestBorderless();
    TestFramePeriod();
    TestShaderConstantTable();
    TestShadowResolution();
    TestProjection();
    TestFilmGrain();
    TestSkipMovies();
    TestMovieSkip();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
