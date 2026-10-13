// Tests des fonctions pures du programme DLSS. Code de sortie : nombre d'échecs.
#include <cmath>
#include <cstdio>

#include "Arguments.h"
#include "BmpWriter.h"
#include "MotionCheck.h"

#include <vector>

namespace {

int g_failures = 0;

void Check(bool condition, const char* name) {
    std::printf("  %s %s\n", condition ? "OK   " : "ECHEC", name);
    if (!condition) {
        ++g_failures;
    }
}

}  // namespace

int main() {
    using hca::dlss::ParseArguments;
    const std::vector<std::wstring> full = {
        L"--parent", L"1234", L"--width", L"1920", L"--height", L"1080", L"--color", L"0xC48,0xC4C,0xC50", L"--color-format", L"44",
        L"--color-size", L"8847360", L"--depth", L"0xC38,0xC3C,0xC40", L"--depth-format", L"100", L"--depth-size", L"8847360",
        L"--motion", L"0xD00,0xD04,0xD08", L"--motion-format", L"83", L"--motion-size", L"8847360", L"--semaphore", L"0xC2C", L"--consumed", L"0xC30", L"--vendor", L"0x10DE", L"--device", L"0x2C05", L"--log", L"x.log"};
    const auto parsed = ParseArguments(full);
    Check(parsed && parsed->color.handles[0] == 0xC48 && parsed->color.handles[2] == 0xC50 && parsed->consumed == 0xC30 && parsed->motion.format == 83 && parsed->depth.format == 100 && parsed->width == 1920 &&
              parsed->vendorId == 0x10DE,
          "arguments lus (décimal et hexadécimal)");
    std::vector<std::wstring> withInfo = full;
    withInfo.push_back(L"--info");
    withInfo.push_back(L"0xD10");
    const auto parsedInfo = ParseArguments(withInfo);
    Check(parsed && parsed->info == 0 && parsedInfo && parsedInfo->info == 0xD10,
          "bloc des données d'image facultatif");
    std::vector<std::wstring> withOutput = full;
    for (const wchar_t* item : {L"--output", L"0xE00,0xE04,0xE08", L"--output-format", L"44", L"--output-size", L"8847360",
                                L"--processed", L"0xE0C"}) {
        withOutput.push_back(item);
    }
    const auto parsedOutput = ParseArguments(withOutput);
    Check(parsed && parsed->processed == 0 && parsedOutput && parsedOutput->output.handles[2] == 0xE08 &&
              parsedOutput->output.format == 44 && parsedOutput->processed == 0xE0C,
          "sorties du DLSS et compteur « traitées » facultatifs");
    std::vector<std::wstring> missing = full;
    missing.erase(missing.begin() + 6, missing.begin() + 8);  // sans --color
    Check(!ParseArguments(missing), "paramètre manquant refusé");
    Check(!ParseArguments({L"--width", L"abc"}), "nombre invalide refusé");
    std::vector<std::wstring> shortList = full;
    shortList[7] = L"0xC48,0xC4C";
    Check(!ParseArguments(shortList), "liste de handles incomplète refusée");
    using hca::dlss::HalfToFloat;
    Check(HalfToFloat(0x3C00) == 1.0f && HalfToFloat(0xC000) == -2.0f && HalfToFloat(0x0000) == 0.0f &&
              HalfToFloat(0x3800) == 0.5f,
          "demi-flottants convertis");
    // Image précédente = image courante décalée de 5 pixels : un pixel en (x, y) était en (x + 5, y).
    {
        const std::uint32_t w = 64;
        const std::uint32_t h = 16;
        std::vector<std::uint8_t> current(w * h * 4);
        std::vector<std::uint8_t> previous(w * h * 4);
        std::vector<std::uint16_t> motion(w * h * 2, 0);
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                const auto v = static_cast<std::uint8_t>((x * 37) % 251);
                for (int c = 0; c < 3; ++c) {
                    current[(y * w + x) * 4 + c] = v;
                }
                if (x + 5 < w) {
                    for (int c = 0; c < 3; ++c) {
                        previous[(y * w + x + 5) * 4 + c] = v;
                    }
                }
                motion[(y * w + x) * 2] = 0x4500;  // 5,0 en demi-flottant
            }
        }
        using hca::dlss::MeanWarpError;
        const float none = MeanWarpError(current.data(), previous.data(), motion.data(), w, h, false, 1, 1, 1);
        const float right = MeanWarpError(current.data(), previous.data(), motion.data(), w, h, true, 1, 1, 1);
        const float wrong = MeanWarpError(current.data(), previous.data(), motion.data(), w, h, true, -1, 1, 1);
        Check(right == 0.0f && none > 10.0f && wrong > 10.0f, "contrôle des vecteurs : bon signe à écart nul");
        Check(std::fabs(hca::dlss::MeanMotionLength(motion.data(), w, h, 1) - 5.0f) < 1e-4f,
              "longueur moyenne des vecteurs");
    }
    std::printf(g_failures == 0 ? "Tous les tests passent.\n" : "%d échec(s).\n", g_failures);
    return g_failures;
}
