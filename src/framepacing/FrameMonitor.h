#pragma once

#include <windows.h>

#include <cstdint>

namespace shhf::frames {

// Intercepte Sleep dans SilentHill.exe pour mesurer la précision réelle de ses attentes.
void Install(HMODULE gameModule, HMODULE engineModule);

// Applique la résolution de minuteur demandée (TimerResolutionMs). Appelé hors de DllMain.
void LateInit() noexcept;

// Encadrent chaque IDirect3DDevice9::Present. OnAfterPresent renvoie true quand un rapport
// périodique vient d'être écrit dans le journal.
void OnBeforePresent() noexcept;
bool OnAfterPresent() noexcept;

// Après un Reset du périphérique (alt-tab, changement de mode) : ne pas compter l'interruption
// comme une saccade.
void OnDeviceReset() noexcept;

void LogFinalReport() noexcept;

// Pour le détecteur de blocage (autre thread) : horodatage QueryPerformanceCounter de la dernière image présentée
// (0 avant la première), fréquence du compteur et thread qui présente les images.
std::int64_t LastPresentTicks() noexcept;
std::int64_t TicksPerSecond() noexcept;
DWORD RenderThreadId() noexcept;

}  // namespace shhf::frames
