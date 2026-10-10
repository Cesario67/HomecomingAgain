#pragma once

#include <windows.h>

namespace hca::audio {

// Diagnostic audio : le moteur contient les identifiants COM de DirectSound 8 et de XAudio2 (constaté le 10/10/2026)
// et sort en stéréo. Intercepte CoCreateInstance du moteur et journalise l'interface audio créée, puis le format de
// sortie (canaux) : XAudio2 CreateMasteringVoice, DirectSound CreateSoundBuffer. Ne modifie rien.
void InstallAudioProbe(HMODULE engineModule);

}  // namespace hca::audio
