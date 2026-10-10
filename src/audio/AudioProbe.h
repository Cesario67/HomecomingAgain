#pragma once

#include <windows.h>

#include <cstdint>

namespace hca::audio {

// Diagnostic audio : le moteur contient les identifiants COM de DirectSound 8 et de XAudio2 (constaté le 10/10/2026)
// et sort en stéréo. Intercepte CoCreateInstance du moteur et journalise l'interface audio créée, puis le format de
// sortie (canaux) : XAudio2 CreateMasteringVoice, DirectSound CreateSoundBuffer. Applique aussi [Audio] Channels
// (sortie forcée) et [Audio] ChannelFix (ordre des canaux du flux 5.1 du moteur).
//
// Fonction pure (testée par tests/unit) : le flux 5.1 du moteur arrive décalé d'un canal (canal 0 toujours vide,
// mesuré le 10/10/2026). Remet chaque image de 6 échantillons dans l'ordre de Windows : la source 1 devient l'avant gauche,
// 2 l'avant droit, 3 le centre, 4 les basses, 5 l'arrière gauche et 0 l'arrière droit.
void RotateSurroundFrames(float* samples, std::uint32_t frames) noexcept;

void InstallAudioProbe(HMODULE engineModule);

}  // namespace hca::audio
