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
#include "DlssUpscaler.h"
#include "MotionCheck.h"
#include "SharedFrameReader.h"
#include "dlss/FrameInfo.h"

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
    Log("HomecomingAgain.Dlss : démarrage (jalon 4, DLSS en mode DLAA sur les images partagées)");

    const HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parsed->parentPid);
    hca::dlss::SharedFrameReader reader(*parsed, Log);
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> deviceExtensions;
    const bool dlssExtensions = hca::dlss::DlssUpscaler::RequiredExtensions(instanceExtensions, deviceExtensions);
    Log(std::format("DLSS : extensions Vulkan demandées : {} pour l'instance, {} pour le périphérique{}",
                    instanceExtensions.size(), deviceExtensions.size(), dlssExtensions ? "" : " (liste indisponible)"));
    reader.AddExtensions(instanceExtensions, deviceExtensions);
    if (!reader.Open()) {
        Log("ouverture des images partagées impossible : arrêt");
        return 3;
    }

    // Bloc des données de chaque image (décalage de caméra), partagé par le mod.
    const hca::dlss::FrameInfoBlock* info = nullptr;
    if (parsed->info != 0) {
        info = static_cast<const hca::dlss::FrameInfoBlock*>(
            MapViewOfFile(reinterpret_cast<HANDLE>(static_cast<std::uintptr_t>(parsed->info)), FILE_MAP_READ, 0, 0,
                          sizeof(hca::dlss::FrameInfoBlock)));
    }
    Log(std::format("bloc des données d'image : {}", info != nullptr ? "ouvert" : "absent"));

    // DLSS : nvngx_dlss.dll à côté de ce programme, journaux de NGX dans le dossier des journaux.
    wchar_t exePath[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    exeDir = exeDir.substr(0, exeDir.find_last_of(L'\\'));
    std::wstring logDir = parsed->logPath;
    logDir = logDir.find_last_of(L'\\') == std::wstring::npos ? exeDir : logDir.substr(0, logDir.find_last_of(L'\\'));
    std::vector<hca::dlss::SlotImages> slots;
    for (std::size_t i = 0; i < hca::dlss::kSlots; ++i) {
        slots.push_back(reader.Images(i));
    }
    hca::dlss::DlssUpscaler dlss(Log);
    const bool dlssReady = dlss.Initialize(reader.Context(), *parsed, slots, exeDir, logDir);
    std::uint32_t evaluated = 0;
    std::uint32_t failed = 0;
    std::uint32_t withJitter = 0;
    double evaluateMs = 0.0;
    bool firstEvaluation = true;
    bool outputSaved = false;

    std::uint64_t value = 0;
    std::uint32_t received = 0;
    std::uint32_t total = 0;
    bool saved = false;
    std::uint32_t nextMotionCheck = 180;
    ULONGLONG nextReport = GetTickCount64() + 5000;
    while (parent == nullptr || WaitForSingleObject(parent, 0) == WAIT_TIMEOUT) {
        const std::uint64_t next = reader.WaitNext(value, 100);
        if (next != value) {
            received += static_cast<std::uint32_t>(next - value);
            total += static_cast<std::uint32_t>(next - value);
            const bool consecutive = next == value + 1;
            value = next;
            // Contrôle des vecteurs de mouvement toutes les 300 images (environ 5 s), sur deux images consécutives (la
            // précédente est encore dans son emplacement : les emplacements sont rendus au mod avec une image de
            // retard, voir plus bas). Les vecteurs de l'image v se rapportent à l'image v - 1 transmise, même quand le
            // mod a sauté des images du jeu entre les deux.
            // Pas pendant le retour de l'image : la relecture retarderait le jeu, dont la carte attend le DLSS.
            if (parsed->processed == 0 && total >= nextMotionCheck && consecutive) {
                std::vector<std::uint8_t> previousColor;
                std::vector<std::uint8_t> color;
                std::vector<float> depth;
                std::vector<std::uint16_t> motion;
                if (reader.ReadBack(static_cast<std::size_t>((value - 1) % hca::dlss::kSlots), previousColor, depth,
                                    motion) &&
                    reader.ReadBack(static_cast<std::size_t>(value % hca::dlss::kSlots), color, depth, motion)) {
                    nextMotionCheck = total + 300;
                    using hca::dlss::MeanWarpError;
                    const std::uint32_t w = parsed->width;
                    const std::uint32_t h = parsed->height;
                    const float none = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, false, 1, 1, 4);
                    const float pp = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, 1, 1, 4);
                    const float np = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, -1, 1, 4);
                    const float pn = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, 1, -1, 4);
                    const float nn = MeanWarpError(color.data(), previousColor.data(), motion.data(), w, h, true, -1, -1, 4);
                    const float length = hca::dlss::MeanMotionLength(motion.data(), w, h, 4);
                    Log(std::format("contrôle des vecteurs (image {}, longueur moyenne {:.2f} px ; écart moyen de "
                                    "luminosité avec l'image précédente) : sans vecteurs {:.2f}, avec (+x +y) {:.2f}, "
                                    "(-x +y) {:.2f}, (+x -y) {:.2f}, (-x -y) {:.2f}",
                                    value, length, none, pp, np, pn, nn));
                }
            }
            // Image de contrôle après 2 s d'images reçues (environ 120), lue dans l'emplacement de la dernière image.
            // Relectures lentes (images sur disque) : pas pendant le retour de l'image, la carte du jeu attend le DLSS.
            if (parsed->processed == 0 && !saved && total >= 120) {
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
            // DLSS sur l'image reçue (DLAA). Pas encore de retour vers le jeu : la sortie n'est enregistrée qu'une fois,
            // pour contrôle, avec l'image d'entrée correspondante.
            if (dlssReady) {
                const std::size_t slot = static_cast<std::size_t>(value % hca::dlss::kSlots);
                float jitterX = 0.0f;
                float jitterY = 0.0f;
                bool reset = firstEvaluation || !consecutive;
                if (info != nullptr) {
                    const hca::dlss::FrameInfo& frame = info->frames[slot];
                    if (frame.value == value) {
                        jitterX = frame.jitterX;
                        jitterY = frame.jitterY;
                        reset = reset || frame.reset != 0;
                        withJitter += (jitterX != 0.0f || jitterY != 0.0f) ? 1 : 0;
                    }
                }
                LARGE_INTEGER start{};
                LARGE_INTEGER end{};
                LARGE_INTEGER frequency{};
                QueryPerformanceCounter(&start);
                if (dlss.Evaluate(slot, value, jitterX, jitterY, reset)) {
                    ++evaluated;
                    firstEvaluation = false;
                } else {
                    ++failed;
                }
                QueryPerformanceCounter(&end);
                QueryPerformanceFrequency(&frequency);
                evaluateMs += static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
                              static_cast<double>(frequency.QuadPart);
                if (parsed->processed == 0 && !outputSaved && total >= 300 && evaluated > 0) {
                    outputSaved = true;
                    std::vector<std::uint8_t> output;
                    std::vector<std::uint8_t> input;
                    std::vector<float> depth;
                    std::vector<std::uint16_t> motion;
                    const std::wstring dir = parsed->outputDir.empty() ? L"." : parsed->outputDir;
                    const bool a = dlss.ReadOutput(slot, output) &&
                                   hca::dlss::WriteColorBmp(dir + L"\\dlss_sortie.bmp", output.data(), parsed->width,
                                                            parsed->height);
                    const bool b = reader.ReadBack(slot, input, depth, motion) &&
                                   hca::dlss::WriteColorBmp(dir + L"\\dlss_entree.bmp", input.data(), parsed->width,
                                                            parsed->height);
                    Log(std::format("DLSS : images de contrôle enregistrées : sortie {}, entrée {}", a ? "oui" : "non",
                                    b ? "oui" : "non"));
                }
            }
            // L'emplacement de l'image précédente est rendu au mod (une image de retard, pour que le contrôle des
            // vecteurs puisse relire deux images consécutives).
            if (value > 1) {
                reader.MarkConsumed(value - 1);
            }
        }
        if (GetTickCount64() >= nextReport) {
            nextReport += 5000;
            Log(std::format("images reçues : {} en 5 s (compteur {})", received, value));
            if (dlssReady) {
                Log(std::format("DLSS : {} images traitées ({} avec décalage de caméra), {} échecs, {:.2f} ms par image "
                                "(attente de la carte comprise)",
                                evaluated, withJitter, failed, evaluated > 0 ? evaluateMs / evaluated : 0.0));
                evaluated = 0;
                failed = 0;
                withJitter = 0;
                evaluateMs = 0.0;
            }
            received = 0;
        }
    }
    Log("le jeu s'est fermé : arrêt");
    return 0;
}
