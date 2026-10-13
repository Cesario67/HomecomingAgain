// HomecomingAgain.Dlss.exe : programme 64 bits lancé par le mod (dlss/SharedFrames.cpp). Jalon 3 du DLSS
// (docs/plan-dlss.md) : ouvre les images partagées par le jeu, compte les images reçues et enregistre une image de
// contrôle de la scène et de la profondeur. Se termine avec le jeu.

#include <windows.h>
#include <shellapi.h>

#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "Arguments.h"
#include "BmpWriter.h"
#include "MotionCheck.h"
#include "SharedFrameReader.h"

namespace {

std::ofstream g_log;

void Log(const std::string& message) {
    SYSTEMTIME t{};
    GetLocalTime(&t);
    g_log << std::format("{:02}:{:02}:{:02}.{:03} ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds) << message << '\n';
    g_log.flush();
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::wstring> args(argv + 1, argv + argc);
    LocalFree(argv);
    const auto parsed = hca::dlss::ParseArguments(args);
    if (!parsed) {
        return 2;
    }
    g_log.open(parsed->logPath.empty() ? L"HomecomingAgain.Dlss.log" : parsed->logPath, std::ios::trunc);
    Log("HomecomingAgain.Dlss : démarrage (jalon 3, lecture des images partagées, anneau de 3 et compteur en retour)");

    const HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parsed->parentPid);
    hca::dlss::SharedFrameReader reader(*parsed, Log);
    if (!reader.Open()) {
        Log("ouverture des images partagées impossible : arrêt");
        return 3;
    }

    std::uint64_t value = 0;
    std::uint32_t received = 0;
    std::uint32_t total = 0;
    bool saved = false;
    bool motionChecked = false;
    ULONGLONG nextReport = GetTickCount64() + 5000;
    while (parent == nullptr || WaitForSingleObject(parent, 0) == WAIT_TIMEOUT) {
        const std::uint64_t next = reader.WaitNext(value, 100);
        if (next != value) {
            received += static_cast<std::uint32_t>(next - value);
            total += static_cast<std::uint32_t>(next - value);
            const bool consecutive = next == value + 1;
            value = next;
            // Contrôle des vecteurs de mouvement, sur deux images consécutives (la précédente est encore dans son
            // emplacement : les emplacements sont rendus au mod avec une image de retard, voir plus bas).
            if (!motionChecked && total >= 180 && consecutive) {
                std::vector<std::uint8_t> previousColor;
                std::vector<std::uint8_t> color;
                std::vector<float> depth;
                std::vector<std::uint16_t> motion;
                if (reader.ReadBack(static_cast<std::size_t>((value - 1) % hca::dlss::kSlots), previousColor, depth,
                                    motion) &&
                    reader.ReadBack(static_cast<std::size_t>(value % hca::dlss::kSlots), color, depth, motion)) {
                    motionChecked = true;
                    using hca::dlss::MeanWarpError;
                    const std::uint32_t w = parsed->width;
                    const std::uint32_t h = parsed->height;
                    const float none = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, false, 1, 1, 4);
                    const float pp = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, 1, 1, 4);
                    const float np = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, -1, 1, 4);
                    const float pn = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, 1, -1, 4);
                    const float nn = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, -1, -1, 4);
                    Log(std::format("contrôle des vecteurs (écart moyen de luminosité avec l'image précédente) : sans "
                                    "vecteurs {:.2f}, avec (+x +y) {:.2f}, (-x +y) {:.2f}, (+x -y) {:.2f}, (-x -y) {:.2f}",
                                    none, pp, np, pn, nn));
                }
            }
            // Image de contrôle après 2 s d'images reçues (environ 120), lue dans l'emplacement de la dernière image.
            if (!saved && total >= 120) {
                saved = true;
                std::vector<std::uint8_t> color;
                std::vector<float> depth;
                std::vector<std::uint16_t> motion;
                if (reader.ReadBack(static_cast<std::size_t>(value % hca::dlss::kSlots), color, depth, motion)) {
                    const std::wstring dir = parsed->outputDir.empty() ? L"." : parsed->outputDir;
                    const bool a = hca::dlss::WriteColorBmp(dir + L"\\dlss_scene.bmp", color.data(), parsed->width,
                                                            parsed->height);
                    const bool b = hca::dlss::WriteDepthBmp(dir + L"\\dlss_profondeur.bmp", depth.data(),
                                                            parsed->width, parsed->height);
                    float average = 0.0f;
                    float maximum = 0.0f;
                    const bool m = hca::dlss::WriteMotionBmp(dir + L"\\dlss_mouvement.bmp", motion.data(), parsed->width,
                                                             parsed->height, average, maximum);
                    Log(std::format("images de contrôle enregistrées : scène {}, profondeur {}, mouvement {} (moyenne {:.2f} px, "
                                    "maximum {:.2f} px)",
                                    a ? "oui" : "non", b ? "oui" : "non", m ? "oui" : "non", average, maximum));
                } else {
                    Log("relecture des images impossible");
                }
            }
            // Pas encore de traitement : l'emplacement de l'image précédente est rendu au mod (une image de retard, pour
            // que le contrôle des vecteurs puisse relire deux images consécutives).
            if (value > 1) {
                reader.MarkConsumed(value - 1);
            }
        }
        if (GetTickCount64() >= nextReport) {
            nextReport += 5000;
            Log(std::format("images reçues : {} en 5 s (compteur {})", received, value));
            received = 0;
        }
    }
    Log("le jeu s'est fermé : arrêt");
    return 0;
}
