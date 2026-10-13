// Tests unitaires des fonctions pures du mod (sans jeu ni matériel). Lancés par tests/run-tests.ps1.
// Code de sortie : nombre d'échecs.

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iterator>
#include <string>

#include "audio/AudioProbe.h"
#include "compat/MovieSkip.h"
#include "compat/SkipMovies.h"
#include "dlss/SharedFrames.h"
#include "core/Strings.h"
#include "graphics/BorderlessWindow.h"
#include "graphics/CameraMath.h"
#include "graphics/CameraTracker.h"
#include "graphics/DepthEncoding.h"
#include "graphics/Jitter.h"
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

void TestChannelFix() {
    std::printf("Canaux audio\n");
    int order[6]{};
    Check(hca::audio::ParseChannelOrder("2,4,1,3,5,0", order) && order[0] == 2 && order[5] == 0, "ordre lu");
    Check(!hca::audio::ParseChannelOrder("2,4,1,3,5", order) && !hca::audio::ParseChannelOrder("2,2,1,3,5,0", order) &&
              !hca::audio::ParseChannelOrder("2,4,1,3,5,9", order),
          "ordre incomplet, en double ou hors limites refusé");
    float frames[6] = {0, 10, 20, 30, 40, 50};
    const int map[6] = {2, 4, 1, 3, 5, 0};
    hca::audio::RemapSurroundFrames(frames, 1, map);
    Check(frames[0] == 20 && frames[1] == 40 && frames[2] == 10 && frames[5] == 0, "canaux réordonnés");
    float mono[6] = {0, 1, 0, 0, 0, 0};
    hca::audio::DownmixSurroundToStereo(mono, 1, false);
    Check(mono[0] == mono[1] && mono[0] > 0.7f, "bruitage du canal 1 centré en stéréo");
    float two[12] = {0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0};
    hca::audio::DownmixSurroundToStereo(two, 2, false);
    Check(two[0] == two[1] && two[2] == two[3] && two[0] > 0.7f && two[2] > 0.7f, "images converties de façon compacte, centrées");
    float side[6] = {0, 1, 0, 0, 0, 0};
    hca::audio::DownmixSurroundToStereo(side, 1, false);
    Check(side[0] == side[1], "son de l'avant droit centré");
    float std51[6] = {1, 0, 0, 0, 0, 1};
    hca::audio::DownmixSurroundToStereo(std51, 1, true);
    Check(std51[0] == 1.0f && std51[1] > 0.7f && std51[1] < 0.71f, "mélange 5.1 habituel : avant gauche à gauche, arrière droit à droite");
}

void TestSharedFrames() {
    std::printf("Partage des images (DLSS)\n");
    Check(hca::dlss::SlotForFrame(0, 3) == 0 && hca::dlss::SlotForFrame(4, 3) == 1 && hca::dlss::SlotForFrame(5, 0) == 0,
          "emplacements tournants");
    using hca::dlss::SlotIsFree;
    Check(SlotIsFree(1, 0, 3) && SlotIsFree(3, 0, 3) && !SlotIsFree(4, 0, 3) && SlotIsFree(4, 1, 3) && !SlotIsFree(7, 3, 3),
          "emplacement libre seulement si l'image précédente est consommée");
}

void TestCameraMath() {
    using namespace hca::graphics;
    std::printf("Matrices de la caméra\n");
    // Projection du relevé (prologue) et vue quelconque (rotation + translation).
    const Matrix4 proj = {1.6407f, 0, 0, 0, 0, 2.9167f, 0, 0, 0, 0, 1.0002f, -3.0006f, 0, 0, 1, 0};
    const Matrix4 view = {0.8f, 0, -0.6f, 10, 0, 1, 0, -2, 0.6f, 0, 0.8f, 30, 0, 0, 0, 1};
    const Matrix4 world = {0, -1, 0, 5, 1, 0, 0, 7, 0, 0, 1, -3, 0, 0, 0, 1};
    const Matrix4 viewProj = Multiply(proj, view);
    const Matrix4 wvp = Multiply(viewProj, world);
    Matrix4 found{};
    bool close = ViewProjectionFromDraw(wvp.data(), world.data(), found);
    for (int i = 0; close && i < 16; ++i) {
        close = std::fabs(found[i] - viewProj[i]) < 1e-3f;
    }
    Check(close, "caméra retrouvée à partir de mWorldViewProj et mWorld");
    Matrix4 inverse{};
    const Matrix4 identity = Multiply(viewProj, (Inverse(viewProj, inverse), inverse));
    Check(std::fabs(identity[0] - 1) < 1e-4f && std::fabs(identity[5] - 1) < 1e-4f && std::fabs(identity[1]) < 1e-4f &&
              std::fabs(identity[15] - 1) < 1e-4f,
          "inverse de la caméra");
    Check(hca::graphics::camera::LooksLikePerspectiveCamera(viewProj), "caméra en perspective reconnue");
    const Matrix4 ortho = {0.001f, 0, 0, -0.999f, 0, -0.002f, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1};
    Check(!hca::graphics::camera::LooksLikePerspectiveCamera(ortho), "projection d'interface 2D rejetée");
    Matrix4 nudged = viewProj;
    nudged[3] += 0.5f;
    Check(hca::graphics::camera::SameCamera(viewProj, viewProj) && !hca::graphics::camera::SameCamera(viewProj, nudged),
          "comparaison de caméras");
    const Matrix4 singular{};
    Check(!Inverse(singular, inverse), "matrice non inversible refusée");
}

// Caméra regardant le long de +z depuis (x, 0, 0), tournée de `degrees` autour de l'axe vertical : vue puis projection
// du jeu (xs 1.6407, ys 2.9167, a 1.00019, b -3.0006).
hca::graphics::Matrix4 TestCamera(float x, float degrees) {
    const float r = degrees * 3.14159265f / 180.0f;
    const float c = std::cos(r);
    const float s = std::sin(r);
    // Vue : lignes = axes droite, haut, regard ; translation = -axe . position.
    const hca::graphics::Matrix4 view = {c, 0, -s, -c * x, 0, 1, 0, 0, s, 0, c, -s * x, 0, 0, 0, 1};
    const hca::graphics::Matrix4 projection = {1.6407f, 0, 0, 0, 0, 2.9167f, 0, 0, 0, 0, 1.00019f, -3.0006f, 0, 0, 1, 0};
    return hca::graphics::Multiply(projection, view);
}

void TestCameraCut() {
    using hca::graphics::IsCameraCut;
    const auto base = TestCamera(100.0f, 0.0f);
    Check(!IsCameraCut(base, base, 15.0f, 20.0f), "même caméra : pas de changement de plan");
    Check(!IsCameraCut(TestCamera(100.8f, 6.0f), base, 15.0f, 20.0f), "rotation de 6 degrés et petit déplacement : continu");
    Check(IsCameraCut(TestCamera(100.0f, 30.0f), base, 15.0f, 20.0f), "rotation de 30 degrés : changement de plan");
    Check(IsCameraCut(TestCamera(150.0f, 0.0f), base, 15.0f, 20.0f), "déplacement de 50 unités : changement de plan");
    const hca::graphics::Matrix4 singular{};
    Check(IsCameraCut(singular, base, 15.0f, 20.0f), "caméra non inversible : changement de plan");
}

void TestDepthEncoding() {
    using hca::graphics::depth::Encoding;
    using hca::graphics::depth::ParseEncoding;
    // Envoi relevé le 13/10/2026 : c98 seul, [0.9 0 0.1 0.9].
    const float c98[4] = {0.9f, 0.0f, 0.1f, 0.9f};
    Encoding encoding;
    Check(ParseEncoding(98, c98, 1, encoding) && encoding.scale == 0.9f && encoding.bias == 0.0f,
          "codage de la profondeur lu dans c98");
    // c98 au milieu d'un envoi plus large.
    float block[3 * 4] = {};
    block[4] = 0.5f;
    block[5] = 0.25f;
    Check(ParseEncoding(97, block, 3, encoding) && encoding.scale == 0.5f && encoding.bias == 0.25f,
          "codage de la profondeur dans un envoi de plusieurs registres");
    Encoding untouched;
    Check(!ParseEncoding(99, c98, 1, untouched) && !ParseEncoding(97, c98, 1, untouched) && untouched.scale == 0.9f,
          "envois sans c98 ignorés");
    const float zero[4] = {};
    Check(!ParseEncoding(98, zero, 1, untouched), "facteur nul refusé");
}

void TestJitter() {
    using namespace hca::graphics::jitter;
    Check(Halton(1, 2) == 0.5f && Halton(2, 2) == 0.25f && Halton(3, 2) == 0.75f &&
              std::fabs(Halton(1, 3) - 1.0f / 3.0f) < 1e-6f && std::fabs(Halton(2, 3) - 2.0f / 3.0f) < 1e-6f,
          "suite de Halton (bases 2 et 3)");
    // 16 phases dans [-0.5, 0.5), de moyenne proche de 0, puis répétition.
    float sumX = 0.0f;
    float sumY = 0.0f;
    bool inRange = true;
    for (std::uint64_t frame = 0; frame < 16; ++frame) {
        const Offset o = OffsetForFrame(frame, 1.0f);
        inRange &= o.x >= -0.5f && o.x < 0.5f && o.y >= -0.5f && o.y < 0.5f;
        sumX += o.x;
        sumY += o.y;
    }
    const Offset first = OffsetForFrame(0, 2.0f);
    const Offset again = OffsetForFrame(16, 2.0f);
    Check(inRange && std::fabs(sumX / 16) < 0.05f && std::fabs(sumY / 16) < 0.05f && first.x == again.x &&
              first.y == again.y && first.x == (Halton(1, 2) - 0.5f) * 2.0f,
          "décalages de caméra : 16 phases centrées, amplitude appliquée");
    // Perspective : un point (x, y, z, w) projeté est déplacé de (dx, dy) en coordonnées normalisées.
    float m[16] = {1.6f, 0, 0, 0, 0, 2.9f, 0, 0, 0, 0, 1.0002f, -3.0006f, 0, 0, 1, 0};
    const float point[4] = {10, -5, 100, 1};
    Check(ApplyToMatrix(m, 0.01f, -0.02f), "décalage appliqué à une perspective");
    const float x = m[0] * point[0] + m[1] * point[1] + m[2] * point[2] + m[3];
    const float y = m[4] * point[0] + m[5] * point[1] + m[6] * point[2] + m[7];
    const float w = m[12] * point[0] + m[13] * point[1] + m[14] * point[2] + m[15];
    Check(std::fabs(x / w - (1.6f * 10 / 100 + 0.01f)) < 1e-5f && std::fabs(y / w - (2.9f * -5 / 100 - 0.02f)) < 1e-5f,
          "décalage constant à l'écran");
    float ortho[16] = {0.001f, 0, 0, -0.999f, 0, -0.002f, 0, 1, 0, 0, 1, 0, 0, 0, 0, 1};
    Check(!ApplyToMatrix(ortho, 0.01f, 0.01f) && ortho[3] == -0.999f, "projection d'interface laissée telle quelle");
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
    TestChannelFix();
    TestSharedFrames();
    TestCameraMath();
    TestCameraCut();
    TestDepthEncoding();
    TestJitter();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
