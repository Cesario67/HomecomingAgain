#pragma once

#include <windows.h>

namespace hca::audio {

// Diagnostic audio : le moteur contient les identifiants COM de DirectSound 8 et de XAudio2 (constaté le 10/10/2026)
// et sort en stéréo. Intercepte CoCreateInstance du moteur et journalise l'interface audio créée, puis le format de
// sortie (canaux) : XAudio2 CreateMasteringVoice, DirectSound CreateSoundBuffer. Applique aussi [Audio] Channels
// (sortie forcée) et [Audio] ChannelFix (ordre des canaux du flux 5.1 du moteur).
//
// Fonction pure (testée par tests/unit) : matrice 6 canaux (flux décalé du moteur) vers 2, 6 ou 8 canaux de sortie,
// rangée [sortie * 6 + source]. matrix doit contenir 48 flottants à zéro. Renvoie false pour un autre nombre de canaux.
bool ChannelFixMatrix(UINT32 outputChannels, float* matrix) noexcept;

void InstallAudioProbe(HMODULE engineModule);

}  // namespace hca::audio
