// Tests des fonctions pures du programme DLSS. Code de sortie : nombre d'échecs.
#include <cstdio>

#include "Arguments.h"
#include "BmpWriter.h"

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
    std::printf(g_failures == 0 ? "Tous les tests passent.\n" : "%d échec(s).\n", g_failures);
    return g_failures;
}
