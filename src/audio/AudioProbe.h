#pragma once

#include <windows.h>

#include <cstdint>

namespace hca::audio {

// Diagnostic audio : le moteur contient les identifiants COM de DirectSound 8 et de XAudio2 (constaté le 10/10/2026)
// et sort en stéréo. Intercepte CoCreateInstance du moteur et journalise l'interface audio créée, puis le format de
// sortie (canaux) : XAudio2 CreateMasteringVoice, DirectSound CreateSoundBuffer. Applique aussi [Audio] Channels
// (sortie forcée) et [Audio] ChannelFix (ordre des canaux du flux 5.1 du moteur).
//
// Fonctions pures (testées par tests/unit). Le flux 5.1 du moteur n'est pas dans l'ordre de Windows (canal 0 toujours
// vide, sons à droite, mesuré le 10/10/2026). order[c] = canal du moteur à placer dans le canal c de Windows (AvG AvD
// C Basses ArG ArD). ParseChannelOrder lit « 2,4,1,3,5,0 » (chaque canal 0 à 5 une seule fois), false sinon.
bool ParseChannelOrder(const char* text, int order[6]) noexcept;
void RemapSurroundFrames(float* samples, std::uint32_t frames, const int order[6]) noexcept;

void InstallAudioProbe(HMODULE engineModule);

}  // namespace hca::audio
