#pragma once

#include <windows.h>

#include <string>

namespace hca::input {

// Diagnostic des manettes : le moteur (g_SilentHill.sgl) lit les manettes par XINPUT1_3.dll, importée par ordinaux
// (vérifié, docs/analyse-shh-exe.md) : 2 = XInputGetState, 3 = XInputSetState, 4 = XInputGetCapabilities. Les trois
// sont interceptées dans l'IAT du moteur et transmises telles quelles ; le journal décrit les manettes interrogées,
// leurs branchements, les premiers changements de boutons et, toutes les 10 s, le rythme de lecture, l'amplitude des
// sticks et les vibrations demandées. Désactivable : `[Input] XInputLog=0`.
void InstallXInputProbe(HMODULE engine);

// Fonctions pures (testées par tests/unit).
// Noms des boutons enfoncés (masque XINPUT_GAMEPAD_*), séparés par des espaces ; « aucun » si vide.
std::string DescribeButtons(WORD buttons);
// Inclinaison d'un stick, de 0 (repos) à 1 (bord).
float StickAmount(SHORT x, SHORT y) noexcept;

}  // namespace hca::input
