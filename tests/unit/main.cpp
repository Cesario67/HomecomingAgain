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
#include "compat/StartupLogos.h"
#include "dlss/SharedFrames.h"
#include "core/Strings.h"
#include "graphics/BorderlessWindow.h"
#include "graphics/CameraMath.h"
#include "graphics/CameraTracker.h"
#include "graphics/DepthEncoding.h"
#include "graphics/Dxt3.h"
#include "graphics/FlashIconArt.h"
#include "graphics/FlashIcons.h"
#include "graphics/Jitter.h"
#include "graphics/UiClip.h"
#include "graphics/SceneEnd.h"
#include "crash/HangClock.h"
#include "input/ButtonIcons.h"
#include "input/ControllerNames.h"
#include "input/MenuBridge.h"
#include "graphics/FilmGrain.h"
#include "graphics/Projection.h"
#include "graphics/ShaderConstantTable.h"
#include "graphics/ShadowResolution.h"
#include "patches/CameraTuning.h"
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
    // Envoi relevé le 13/10/2026 : c92 = [4.5 0 ...] avant le shader des ombres.
    float bias[4] = {4.5f, 0.0f, 1.0f, 1.0f};
    Check(shadows::ScaleCasterBias(92, bias, 1, 8192) && bias[0] == 18.0f && bias[1] == 0.0f,
          "décalage de pente des ombres multiplié par l'agrandissement (8192 : x4)");
    float biasOriginal[4] = {4.5f, 0.0f, 1.0f, 1.0f};
    Check(!shadows::ScaleCasterBias(92, biasOriginal, 1, 2048) && !shadows::ScaleCasterBias(93, biasOriginal, 1, 8192) &&
              biasOriginal[0] == 4.5f,
          "décalage de pente intact en 2048 ou hors de c92");
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

void TestStartupLogos() {
    hca::compat::FirstLoadingScreen screen;
    // Avant les logos (boucle pas encore lancée), pendant, puis fin : seule la fin compte, une seule fois.
    const bool before = screen.Update(false);
    const bool during = screen.Update(true) || screen.Update(true);
    const bool end = screen.Update(false);
    const bool later = screen.Update(true) || screen.Update(false);
    Check(!before && !during && end && !later, "logos : fin de la première boucle de chargement seulement");
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
    Check(std51[0] > 0.7f && std51[0] < 0.71f && std51[1] > 1.4f && std51[1] < 1.42f,
          "mélange 5.1 habituel : arrière droit à droite, avant gauche centré (piste de cinématique d'un seul côté)");
    float frontLeft[6] = {1, 0, 0, 0, 0, 0};
    hca::audio::DownmixSurroundToStereo(frontLeft, 1, true);
    Check(frontLeft[0] == frontLeft[1] && frontLeft[0] > 0.7f, "canal avant gauche seul : égal des deux côtés");
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

// Suites de cibles relevées par F11 le 13/10/2026 (surfaces représentées par des entiers).
void TestSceneEnd() {
    using hca::graphics::SceneEndDetector;
    const int depth = 1, scene = 2, post = 3, shadow = 4, reduced = 5;
    struct Step {
        const int* surface;
        bool full;
    };
    // Shaders en basse qualité : profondeur, scène A8R8G8B8, ombres, flou réduit, profondeur, scène, post-traitement.
    const Step lowQuality[] = {{&depth, true},  {&scene, true}, {&shadow, false}, {&reduced, false},
                               {&scene, true},  {&depth, true}, {&scene, true},   {&post, true}};
    // Shaders en haute qualité : la scène est elle-même en A16B16G16R16F, reflet en 1024x512 avant les ombres.
    const Step highQuality[] = {{&depth, true}, {&scene, true}, {&reduced, false}, {&shadow, false},
                                {&scene, true}, {&depth, true}, {&scene, true},  {&post, true}};
    for (const auto* steps : {lowQuality, highQuality}) {
        SceneEndDetector detector;
        int endedAt = -1;
        for (int i = 0; i < 8; ++i) {
            const bool isDepth = steps[i].surface == &depth;
            if (detector.OnSetRenderTarget(steps[i].surface, isDepth, steps[i].full)) {
                endedAt = endedAt < 0 ? i : 100;
            }
        }
        Check(endedAt == 7 && detector.Scene() == &scene, "fin de scène au passage vers le post-traitement");
    }
    SceneEndDetector detector;
    Check(!detector.OnSetRenderTarget(&scene, false, true) && !detector.OnSetRenderTarget(&post, false, true),
          "pas de fin de scène sans pré-passe de profondeur");
}


void TestMenuPadKeys() {
    using namespace hca::input;
    KeySet held{};
    // Croix bas + Start : flèche bas et Entrée ; A et B ne sont plus traduits.
    KeySet keys = KeysForPad(0x0002 | 0x0010, 0, 0, held);
    Check(keys[kKeyDown] && keys[kKeyReturn] && !keys[kKeyUp] && !keys[kKeyEscape], "croix et Start traduits en touches");
    keys = KeysForPad(0x1000 | 0x2000, 0, 0, held);
    Check(!keys[kKeyEscape] && !keys[kKeyReturn], "A et B non traduits");
    // Stick : enfoncé au-delà de la moitié, relâché seulement sous un tiers (pas de rebond autour du seuil).
    keys = KeysForPad(0, 0, 20000, held);
    const bool pressed = keys[kKeyUp];
    keys = KeysForPad(0, 0, 14000, held);
    const bool stillHeld = keys[kKeyUp];
    keys = KeysForPad(0, 0, 9000, held);
    Check(pressed && stillHeld && !keys[kKeyUp], "stick vers le haut avec hystérésis");
    KeySet fresh{};
    keys = KeysForPad(0, -20000, 0, fresh);
    Check(keys[kKeyLeft] && !keys[kKeyRight] && !KeysForPad(0, 14000, 0, fresh)[kKeyRight],
          "stick à gauche, et à droite sous le seuil d'appui");
    // Menu avant toute scène 3D, puis seulement après 5 s sans scène : un décrochage d'une seconde en jeu ne suffit pas.
    Check(MenuActive(1000, 0) && !MenuActive(11000, 10000) && !MenuActive(15000, 10000) && MenuActive(15001, 10000),
          "menu détecté après 5 s sans scène 3D seulement");
}

void TestControllerNames() {
    using hca::input::ControllerInputLabel;
    const auto is = [](const char* a, const char* b) { return a != nullptr && std::string(a) == b; };
    // Correspondance vérifiée par désassemblage (0x10A40A63) : 6 = LB, 16 = LT, 12 = croix haut ; axe 1 = stick gauche vertical.
    Check(is(ControllerInputLabel(false, 0, 1, true), "A") && is(ControllerInputLabel(false, 6, 1, true), "LB") &&
              is(ControllerInputLabel(false, 16, 1, false), "LT") && is(ControllerInputLabel(false, 12, 1, true), "Croix haut") &&
              is(ControllerInputLabel(false, 7, 2, false), "R1") && is(ControllerInputLabel(true, 1, 1, false), "Left stick (vertical)"),
          "noms des boutons et des axes de la manette");
    Check(ControllerInputLabel(false, 0, 0, true) == nullptr && ControllerInputLabel(false, 4, 1, true) == nullptr &&
              ControllerInputLabel(true, 2, 1, true) == nullptr,
          "noms d'origine gardés en mode PC, boutons 4 et 5 et axes Z");
    using hca::input::ManifestIsFrench;
    Check(ManifestIsFrench("		\"language\"		\"french\"") && !ManifestIsFrench("\"language\" \"english\"") && !ManifestIsFrench(""),
          "langue lue dans le manifeste Steam");
}

void TestButtonIcons() {
    using hca::input::IconTypeFor;
    Check(IconTypeFor(0, true) == 1 && IconTypeFor(0, false) == 0, "icônes auto : Xbox 360 avec manette, PC sans");
    Check(IconTypeFor(1, true) == 0 && IconTypeFor(2, false) == 1 && IconTypeFor(3, true) == 2 && IconTypeFor(9, true) == 0,
          "icônes forcées : PC, Xbox 360, PS3, valeur inconnue = PC");
    using hca::input::FaceIconName;
    Check(std::string(FaceIconName(1, 0)) == "shv_buttonxbox360_a_32" && std::string(FaceIconName(2, 3)) == "shv_buttonps3_triangle_32" &&
              std::string(FaceIconName(0, 1)) == "shv_buttonpccontrol2" && std::string(FaceIconName(1, 6)) == "shv_buttonxbox360_x_32" && std::string(FaceIconName(7, 9)) == "shv_buttonpccontrol1",
          "noms des icônes A, B, X, Y selon le type");
    using hca::input::ReplacementIconName;
    const auto same = [](const char* a, const char* b) { return a != nullptr && std::string(a) == b; };
    Check(same(ReplacementIconName("shv_buttonpccontrol1", 1), "shv_buttonxbox360_a_32") &&
              same(ReplacementIconName("shv_buttonpccontrol4_64", 2), "shv_buttonps3_triangle_32") &&
              ReplacementIconName("pc_3flash", 1) == nullptr && ReplacementIconName("pc_1flash", 2) == nullptr,
          "texture PC remplacée par l'icône Xbox 360 ou PS3");
    Check(ReplacementIconName("shv_buttonpccontrol1", 0) == nullptr && ReplacementIconName("shv_buttonactionpc", 1) == nullptr &&
              ReplacementIconName("shv_buttonpccontrol12", 1) == nullptr && ReplacementIconName("pc_1flash_64", 1) == nullptr &&
              ReplacementIconName(nullptr, 1) == nullptr,
          "autres textures et type PC laissés tels quels");
    {
        // Icônes animées de l'invite à marteler (graphics/FlashIcons, FlashIconArt, Dxt3).
        namespace fi = hca::graphics::flashicons;
        int button = -1;
        int frame = -1;
        Check(fi::IdentifyFlashFrame(0x62400812, button, frame) && button == 0 && frame == 0 &&
                  fi::IdentifyFlashFrame(0xC782C21B, button, frame) && button == 3 && frame == 1 &&
                  !fi::IdentifyFlashFrame(0x12345678, button, frame),
              "images pc_1flash à pc_4flash reconnues à leur empreinte");
        const std::uint8_t abc[3] = {'a', 'b', 'c'};
        Check(fi::Fnv1a(abc, 3) == 0x1A47E90Bu && fi::Fnv1a(nullptr, 0) == 2166136261u, "FNV-1a (valeurs de référence)");

        constexpr int kSize = hca::graphics::kFlashIconSize;
        std::uint32_t source[kSize * kSize];
        std::uint32_t decoded[kSize * kSize];
        std::uint8_t compressed[kSize * kSize];
        for (int y = 0; y < kSize; ++y) {
            for (int x = 0; x < kSize; ++x) {
                source[y * kSize + x] = x < kSize / 2 ? 0xFF2060C0u : 0x00000000u;  // bleu opaque / transparent
            }
        }
        hca::graphics::EncodeDxt3(source, kSize, kSize, compressed);
        hca::graphics::DecodeDxt3(compressed, kSize, kSize, decoded);
        const auto closeEnough = [](std::uint32_t a, std::uint32_t b) {
            for (int shift = 0; shift < 32; shift += 8) {
                const int d = static_cast<int>((a >> shift) & 255) - static_cast<int>((b >> shift) & 255);
                if (d > 10 || d < -10) {
                    return false;
                }
            }
            return true;
        };
        Check(hca::graphics::Dxt3Size(kSize, kSize) == 4096 && closeEnough(decoded[5 * kSize + 5], 0xFF2060C0u) &&
                  (decoded[5 * kSize + 60] >> 24) == 0,
              "DXT3 : couleur unie et transparence conservées");

        std::uint32_t big[kSize * kSize];
        std::uint32_t smallFrame[kSize * kSize];
        const auto coverage = [](const std::uint32_t* pixels) {
            int count = 0;
            for (int i = 0; i < kSize * kSize; ++i) {
                count += (pixels[i] >> 24) > 127 ? 1 : 0;
            }
            return count;
        };
        bool distinct = true;
        std::uint32_t previous = 0;
        for (int type = 1; type <= 2; ++type) {
            for (int b = 0; b < 4; ++b) {
                hca::graphics::RenderFlashFrame(type, b, 0, big);
                hca::graphics::RenderFlashFrame(type, b, 1, smallFrame);
                distinct = distinct && (big[0] >> 24) == 0 && (big[32 * kSize + 32] >> 24) == 255 &&
                           coverage(smallFrame) < coverage(big) && coverage(smallFrame) > 400;
                std::uint32_t hash = fi::Fnv1a(reinterpret_cast<const std::uint8_t*>(big), sizeof(big));
                distinct = distinct && hash != previous;
                previous = hash;
            }
        }
        Check(distinct, "icônes animées : coin transparent, centre opaque, petite image plus petite, huit dessins différents");
        hca::graphics::RenderFlashFrame(0, 0, 0, big);
        Check(coverage(big) == 0, "type PC : rien dessiné");
    }
    using hca::input::ExtraCommandIconName;
    Check(same(ExtraCommandIconName(0x44, 1), "shv_buttonstickx360_r") && same(ExtraCommandIconName(0x44, 2), "shv_buttonps3stickr") &&
              ExtraCommandIconName(0x44, 0) == nullptr && ExtraCommandIconName(0x36, 1) == nullptr &&
              ExtraCommandIconName(8, 1) == nullptr && same(hca::input::PadActionIconName("shv_buttonactionpc", 1), "shv_buttonxboxlb") &&
              same(hca::input::PadActionIconName("shv_buttonactionpc", 2), "shv_buttonps3l1") &&
              hca::input::PadActionIconName("shv_buttonactionpc", 0) == nullptr && hca::input::PadActionIconName("shv_buttonpccontrol1", 1) == nullptr &&
              same(ExtraCommandIconName(0x13, 1), "shv_buttonstickx360_r") && same(ExtraCommandIconName(0x14, 2), "shv_buttonps3stickr"),
          "icône du stick droit pour le zoom de la carte (axes et R3), rien pour le type PC ni les autres commandes");
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

void TestUiClip() {
    using hca::graphics::uiclip::ExpandLegacyRect;
    long w = 1280;
    long h = 720;
    const bool expanded = ExpandLegacyRect(0, 0, w, h, 1920, 1080) && w == 1920 && h == 1080;
    long w720 = 1280;
    long h720 = 720;
    long wOther = 1024;
    long hOther = 1024;
    long wShifted = 1280;
    long hShifted = 720;
    Check(expanded && !ExpandLegacyRect(0, 0, w720, h720, 1280, 720) && !ExpandLegacyRect(0, 0, wOther, hOther, 1920, 1080) &&
              !ExpandLegacyRect(10, 0, wShifted, hShifted, 1920, 1080) && w720 == 1280 && wOther == 1024,
          "carte : zone 1280x720 étendue à la cible, autres zones intactes");
    // Matrice relevée sur la carte (16/10/2026) et un sommet : même pixel en 1280x720 d'origine et en 1920x1080 compensé.
    const float original[16] = {0.84375f, 0, 0, 25, 0, 1.1325e-07f, 1.5f, -25, 0, -1.5f, 1.1325e-07f, 49.001f, 0, -1.5f, 1.1325e-07f, 50};
    float compensated[16];
    std::memcpy(compensated, original, sizeof(compensated));
    hca::graphics::uiclip::CompensateProjection(compensated, 1280.0f / 1920.0f, 720.0f / 1080.0f);
    const float vertex[4] = {12.0f, -7.0f, 3.0f, 1.0f};
    const auto pixel = [&vertex](const float* m, float width, float height, float& px, float& py) {
        float clip[4];
        for (int r = 0; r < 4; ++r) {
            clip[r] = m[r * 4] * vertex[0] + m[r * 4 + 1] * vertex[1] + m[r * 4 + 2] * vertex[2] + m[r * 4 + 3] * vertex[3];
        }
        px = (clip[0] / clip[3] + 1.0f) * 0.5f * width;
        py = (1.0f - clip[1] / clip[3]) * 0.5f * height;
    };
    float x720 = 0, y720 = 0, x1080 = 0, y1080 = 0;
    pixel(original, 1280, 720, x720, y720);
    pixel(compensated, 1920, 1080, x1080, y1080);
    Check(std::fabs(x720 - x1080) < 0.01f && std::fabs(y720 - y1080) < 0.01f && std::fabs(x720 - 640.0f) > 1.0f,
          "carte : même pixel après extension de la zone (projection compensée)");

    // Couteau en inspection, relevé F11 en 1920x1080 (20/10/2026) : le moteur agrandit le modèle de 1,5 dans les
    // quatre lignes sans éloigner la caméra. Après correction, la norme de la ligne w vaut 1, les lignes x et y et les
    // constantes sont intactes.
    const float knife[16] = {0.21838f, 0,       0.815f,   6.5468f, 0,        1.5f,    0,       -10.537f,
                             -1.4489f, 0,       0.38823f, 12.045f, -1.4489f, 0,       0.38823f, 13.045f};
    float fixed[16];
    std::memcpy(fixed, knife, sizeof(fixed));
    const bool changed = hca::graphics::uiclip::RestoreObjectPerspective(fixed, 1.5f);
    const float wNorm = std::sqrt(fixed[12] * fixed[12] + fixed[13] * fixed[13] + fixed[14] * fixed[14]);
    bool keptRows = true;
    for (int i : {0, 1, 2, 3, 4, 5, 6, 7, 11, 15}) {
        keptRows = keptRows && fixed[i] == knife[i];
    }
    Check(changed && std::fabs(wNorm - 1.0f) < 1e-3f && keptRows && std::fabs(fixed[12] - knife[12] / 1.5f) < 1e-5f,
          "objets : perspective de l'inspection rétablie (échelle 1,5 retirée de z et w, x, y et constantes intacts)");
    // La carte (norme 1,5 mais distance 50) et le 720p (échelle 1) ne bougent pas.
    float map[16];
    std::memcpy(map, original, sizeof(map));
    float knife720[16] = {0.1456f, 0, 0.5433f, 0.0468f, 0, 1, 0, -4.037f, -0.966f, 0, 0.2588f, 12.045f, -0.966f, 0, 0.2588f, 13.045f};
    float knife720Copy[16];
    std::memcpy(knife720Copy, knife720, sizeof(knife720));
    Check(!hca::graphics::uiclip::RestoreObjectPerspective(map, 1.5f) && std::memcmp(map, original, sizeof(map)) == 0 &&
              !hca::graphics::uiclip::RestoreObjectPerspective(knife720, 1.0f) &&
              std::memcmp(knife720, knife720Copy, sizeof(knife720)) == 0,
          "objets : carte (distance 50) et 720p (échelle 1) intacts");

    // Projecteur des photos du journal, relevés F11 du 24/10/2026 : position c20 en 1080p, 1440p et 720p.
    using hca::graphics::uiclip::RestoreInspectionLight;
    float light1080[4] = {-22.222f, 12.5f, -25.0f, 1.0071e-28f};
    float light1440[4] = {-44.444f, 25.0f, -25.0f, 1.0071e-28f};
    float light720[4] = {-0.0f, -0.0f, -25.0f, 1.0071e-28f};
    float lightOther[4] = {-3.0f, 2.0f, -25.0f, 0.0f};
    const bool fixed1080 = RestoreInspectionLight(light1080, 1920, 1080);
    const bool fixed1440 = RestoreInspectionLight(light1440, 2560, 1440);
    Check(fixed1080 && fixed1440 && light1080[0] == 0.0f && light1080[1] == 0.0f && light1080[2] == -25.0f &&
              light1440[0] == 0.0f && light1440[1] == 0.0f && light1440[2] == -25.0f,
          "photos : projecteur recentré en 1080p et 1440p (z intact)");
    Check(!RestoreInspectionLight(light720, 1280, 720) && !RestoreInspectionLight(lightOther, 1920, 1080) &&
              lightOther[0] == -3.0f && lightOther[1] == 2.0f,
          "photos : 720p et autre lumière intacts");
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

void TestCameraTuning() {
    using namespace hca::patches::camera;
    std::printf("Correctif de caméra\n");
    Check(ClampSpeed(0.1f) == 0.25f && ClampSpeed(5.0f) == 3.0f && ClampSpeed(1.5f) == 1.5f &&
              ClampSpeed(std::nanf("")) == 1.0f,
          "vitesses bornées à [0,25 ; 3], valeur non finie ramenée à 1");
    const CameraGains defaults = ComputeGains({});
    Check(defaults.yawRadiansPerSecond == 1.5f && std::fabs(defaults.padPitchGain - 171.88733987074934) < 1e-6 &&
              std::fabs(defaults.pitchGain - 2.8647889f) < 1e-6f,
          "réglages à 1 : 1,5 rad/s et gain vertical 2,8648 du Camera Fix");
    const CameraGains fast = ComputeGains({2.0f, 0.5f, 3.0f, 1.5f});
    Check(fast.yawRadiansPerSecond == 3.0f && std::fabs(fast.pitchGain - 1.4323945f) < 1e-6f &&
              fast.aimOutputX == 1.0f && fast.aimOutputY == 0.5f && fast.aimInputGain == 2.0f,
          "réglages appliqués, visée = réglage / 3");
    Check(!IsStickDeflected(0.05f) && !IsStickDeflected(-0.04f) && IsStickDeflected(0.06f) && IsStickDeflected(-1.0f),
          "zone morte de 5 % du stick");
    Check(!IsRawStickDeflected(1638) && IsRawStickDeflected(1700) && IsRawStickDeflected(-32768),
          "zone morte sur l'axe XInput brut");
    Check(std::fabs(TurnDegrees(1.0f, 1.0f, 1.5f) + 85.943671f) < 1e-3f && TurnDegrees(-0.5f, 0.1f, 2.0f) > 0.0f &&
              TurnDegrees(0.0f, 1.0f, 1.5f) == 0.0f,
          "rotation par image en degrés, signe du Camera Fix");
    Check(std::fabs(HeadingFromForward(1.0f, 0.0f)) < 1e-6f &&
              std::fabs(HeadingFromForward(0.0f, 1.0f) - 1.5707963f) < 1e-6f &&
              std::fabs(HeadingFromForward(0.0f, -1.0f) - 4.712389f) < 1e-5f,
          "cap dans [0 ; 2π[");
    Check(IsPrecisionCameraName("Precision") && !IsPrecisionCameraName("PrecisionX") &&
              !IsPrecisionCameraName("Navigation") && !IsPrecisionCameraName("Prec") && !IsPrecisionCameraName(nullptr),
          "caméra de visée reconnue par son nom exact");
    Check(ParseCameraMode(L"Orbit") == CameraMode::Orbit && ParseCameraMode(L"orbit") == CameraMode::Orbit &&
              ParseCameraMode(L"Follow") == CameraMode::Follow && ParseCameraMode(L"") == CameraMode::Follow &&
              ParseCameraMode(L"Orbite") == CameraMode::Follow && ParseCameraMode(nullptr) == CameraMode::Follow,
          "mode de caméra : Orbit reconnu, Follow par défaut");
    Check(std::fabs(WrapHeading(7.0f) - 0.7168147f) < 1e-5f && std::fabs(WrapHeading(-0.5f) - 5.7831855f) < 1e-5f &&
              WrapHeading(1.0f) == 1.0f && WrapHeading(std::nanf("")) == 0.0f,
          "cap ramené dans [0 ; 2π[");
    Check(OrbitYaw(1.0f, 0.04f, 1.0f, 1.5f) == 1.0f && std::fabs(OrbitYaw(1.0f, 1.0f, 0.5f, 1.5f) - 0.25f) < 1e-6f &&
              std::fabs(OrbitYaw(0.1f, 1.0f, 0.2f, 1.5f) - 6.0831853f) < 1e-5f &&
              std::fabs(OrbitYaw(1.0f, -1.0f, 0.5f, 1.5f) - 1.75f) < 1e-6f,
          "orbite : zone morte, sens du mode Follow, passage par zéro");
    Check(OrbitNeedsReset(0, 1000) && !OrbitNeedsReset(1000, 1400) && OrbitNeedsReset(1000, 1600) &&
              OrbitNeedsReset(2000, 1000),
          "orbite : repart du cap du moteur après une interruption de plus de 500 ms");
    Check(std::fabs(SignedAngle(3.5f) + 2.7831853f) < 1e-5f && std::fabs(SignedAngle(-0.5f) + 0.5f) < 1e-6f &&
              std::fabs(SignedAngle(0.5f) - 0.5f) < 1e-6f,
          "angle signé dans [-π ; π[");
    Check(StickMagnitude(0.6f, 0.8f) == 1.0f && std::fabs(StickMagnitude(0.3f, 0.4f) - 0.5f) < 1e-6f &&
              StickMagnitude(std::nanf(""), 0.0f) == 0.0f,
          "inclinaison du stick bornée à 1");
    Check(TurnStep(0.05f, 0.1f) == 0.05f && TurnStep(2.0f, 0.1f) == 0.1f && TurnStep(-2.0f, 0.1f) == -0.1f,
          "rotation par image bornée");
    Check(!IsTriggerPressed(0) && !IsTriggerPressed(30) && IsTriggerPressed(31) && IsTriggerPressed(255),
          "gâchette gauche tenue au-delà du seuil de 30");
}

}  // namespace

void TestHangClock() {
    using hca::crash::HangElapsed;
    // Dernière image à 100, jamais en arrière-plan : 400 ticks sans image.
    Check(HangElapsed(500, 100, 0) == 400, "blocage : temps depuis la dernière image");
    // Alt-Tab de 100 à 450 sans image : seul le temps depuis le retour au premier plan compte.
    Check(HangElapsed(500, 100, 450) == 50, "blocage : temps en arrière-plan ignoré");
    Check(HangElapsed(500, 600, 0) == 0, "blocage : image plus récente que la mesure");
}

int main() {
    TestHangClock();
    TestStrings();
    TestBorderless();
    TestFramePeriod();
    TestShaderConstantTable();
    TestShadowResolution();
    TestProjection();
    TestFilmGrain();
    TestSkipMovies();
    TestMovieSkip();
    TestStartupLogos();
    TestChannelFix();
    TestSharedFrames();
    TestCameraMath();
    TestCameraCut();
    TestSceneEnd();
    TestMenuPadKeys();
    TestButtonIcons();
    TestControllerNames();
    TestDepthEncoding();
    TestJitter();
    TestUiClip();
    TestCameraTuning();
    std::printf(g_failures == 0 ? "Tous les tests unitaires passent.\n" : "%d test(s) unitaire(s) en échec.\n", g_failures);
    return g_failures;
}
