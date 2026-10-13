#include "audio/AudioProbe.h"

#include <intrin.h>
#include <mmreg.h>
#include <objbase.h>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <format>
#include <string>

#include "core/Config.h"
#include "core/Hooking.h"
#include "core/Log.h"

namespace hca::audio {

namespace {

// Identifiants relevés dans g_SilentHill.sgl (recherche des octets des GUID, 10/10/2026).
constexpr GUID kClsidDirectSound8 = {0x3901CC3F, 0x84B5, 0x4FA4, {0xBA, 0x35, 0xAA, 0x81, 0x72, 0xB8, 0xA0, 0x9B}};
constexpr GUID kClsidDirectSound = {0x47D4D946, 0x62E8, 0x11CF, {0x93, 0xBC, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00}};

using CoCreateInstanceFn = HRESULT(WINAPI*)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
CoCreateInstanceFn g_coCreateInstance = nullptr;

// IXAudio2 (2.x) : CreateMasteringVoice est la 10e méthode (IUnknown 3, GetDeviceCount, GetDeviceDetails, Initialize,
// RegisterForCallbacks, UnregisterForCallbacks, CreateSourceVoice, CreateSubmixVoice, CreateMasteringVoice), index 10.
// Signature 2.7 : (this, IXAudio2MasteringVoice**, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags,
// UINT32 DeviceIndex, const XAUDIO2_EFFECT_CHAIN*).
constexpr size_t kCreateMasteringVoiceIndex = 10;
using CreateMasteringVoiceFn = HRESULT(STDMETHODCALLTYPE*)(void*, void**, UINT32, UINT32, UINT32, UINT32, const void*);
CreateMasteringVoiceFn g_createMasteringVoice = nullptr;

// IDirectSound8 : CreateSoundBuffer est l'index 3 (après IUnknown). Signature :
// (this, LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, LPUNKNOWN). DSBUFFERDESC : dwSize, dwFlags, dwBufferBytes, dwReserved,
// lpwfxFormat (WAVEFORMATEX*), guid3DAlgorithm.
constexpr size_t kCreateSoundBufferIndex = 3;
using CreateSoundBufferFn = HRESULT(STDMETHODCALLTYPE*)(void*, const void*, void**, void*);
CreateSoundBufferFn g_createSoundBuffer = nullptr;
int g_buffersLogged = 0;

// IXAudio2::CreateSourceVoice (index 8) : (this, IXAudio2SourceVoice**, const WAVEFORMATEX*, UINT32 Flags,
// float MaxFrequencyRatio, IXAudio2VoiceCallback*, const XAUDIO2_VOICE_SENDS*, const XAUDIO2_EFFECT_CHAIN*).
// IXAudio2::CreateSubmixVoice (index 9) : (this, IXAudio2SubmixVoice**, UINT32 InputChannels, UINT32 InputSampleRate,
// UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS*, const XAUDIO2_EFFECT_CHAIN*).
constexpr size_t kCreateSourceVoiceIndex = 8;
constexpr size_t kCreateSubmixVoiceIndex = 9;
using CreateSourceVoiceFn = HRESULT(STDMETHODCALLTYPE*)(void*, void**, const WAVEFORMATEX*, UINT32, float, void*,
                                                        const void*, const void*);
using CreateSubmixVoiceFn = HRESULT(STDMETHODCALLTYPE*)(void*, void**, UINT32, UINT32, UINT32, UINT32, const void*,
                                                        const void*);
CreateSourceVoiceFn g_createSourceVoice = nullptr;
CreateSubmixVoiceFn g_createSubmixVoice = nullptr;
int g_sourcesLogged = 0;

// IXAudio2Voice::SetOutputMatrix. Hypothèse pour XAudio2 2.1 : index 14 (GetVoiceDetails, SetOutputVoices,
// SetEffectChain, EnableEffect, DisableEffect, GetEffectState, SetEffectParameters, GetEffectParameters,
// SetFilterParameters, GetFilterParameters, SetVolume, GetVolume, SetChannelVolumes, GetChannelVolumes,
// SetOutputMatrix ; les filtres de sortie n'existent qu'à partir de 2.4). À valider par la cohérence des valeurs.
// Signature : (this, IXAudio2Voice* destination, UINT32 sourceChannels, UINT32 destinationChannels,
// const float* levelMatrix, UINT32 operationSet).
constexpr size_t kSetOutputMatrixIndex = 14;
using SetOutputMatrixFn = HRESULT(STDMETHODCALLTYPE*)(void*, void*, UINT32, UINT32, const float*, UINT32);
SetOutputMatrixFn g_setOutputMatrix = nullptr;
int g_matricesLogged = 0;

HRESULT STDMETHODCALLTYPE HookSetOutputMatrix(void* voice, void* destination, UINT32 sourceChannels,
                                              UINT32 destinationChannels, const float* matrix, UINT32 operationSet) {
    if (g_matricesLogged < 40 && matrix != nullptr && sourceChannels <= 8 && destinationChannels <= 8) {
        ++g_matricesLogged;
        std::string values;
        for (UINT32 i = 0; i < sourceChannels * destinationChannels; ++i) {
            values += std::format(" {:.2f}", matrix[i]);
        }
        log::Info("Audio : SetOutputMatrix voix {} -> {} : {} canal(aux) vers {}, niveaux{}", voice, destination,
                  sourceChannels, destinationChannels, values);
    }
    return g_setOutputMatrix(voice, destination, sourceChannels, destinationChannels, matrix, operationSet);
}

// IXAudio2SourceVoice::SubmitSourceBuffer, hypothèse 2.1 : index 19 (17 méthodes de IXAudio2Voice, puis Start, Stop).
// XAUDIO2_BUFFER : Flags, AudioBytes, pAudioData, PlayBegin, PlayLength, LoopBegin, LoopLength, LoopCount, pContext.
constexpr size_t kSubmitSourceBufferIndex = 19;
using SubmitSourceBufferFn = HRESULT(STDMETHODCALLTYPE*)(void*, const void*, const void*);
SubmitSourceBufferFn g_submitSourceBuffer = nullptr;
void* g_surroundVoice = nullptr;  // la voix 6 canaux du moteur
UINT32 g_masterChannels = 2;      // canaux de la voix maîtresse
double g_cross[6][6]{};
ULONGLONG g_nextCorrelationLog = 0;
int g_correlationLogs = 0;
bool g_surroundAsStereo = false;   // voix 5.1 du moteur créée en stéréo, conversion faite par le mod

// Mesure d'énergie par canal pour chaque voix source du moteur (il n'en crée que trois : 1, 2 et 6 canaux).
struct VoiceMeter {
    void* voice = nullptr;
    UINT32 channels = 0;
    double energy[8]{};
    std::uint64_t frames = 0;
};
VoiceMeter g_meters[4];
ULONGLONG g_nextEnergyLog = 0;
int g_energyLogs = 0;

// XAUDIO2_BUFFER complet (32 bits) : Flags, AudioBytes, pAudioData, PlayBegin, PlayLength, LoopBegin, LoopLength,
// LoopCount, pContext.
struct XAudio2BufferFull {
    UINT32 flags;
    UINT32 audioBytes;
    const BYTE* audioData;
    UINT32 playBegin;
    UINT32 playLength;
    UINT32 loopBegin;
    UINT32 loopLength;
    UINT32 loopCount;
    void* context;
};

struct XAudio2Buffer {
    UINT32 flags;
    UINT32 audioBytes;
    const BYTE* audioData;
};

void TrackVoice(void* voice, UINT32 channels) {
    for (VoiceMeter& meter : g_meters) {
        if (meter.voice == nullptr) {
            meter.voice = voice;
            meter.channels = channels;
            return;
        }
    }
}

// Objet de sortie audio du moteur : le code appelant (0x1084A296, build 6.30) le garde dans ebx au moment d'appeler
// SubmitSourceBuffer. Le relais ci-dessous le note avant de passer la main au hook (étape 1 de la stéréo).
extern "C" void* g_engineSpeaker = nullptr;
HRESULT STDMETHODCALLTYPE HookSubmitSourceBuffer(void* voice, const void* buffer, const void* wma);

extern "C" __declspec(naked) void SubmitSourceBufferRelay() {
    __asm { mov g_engineSpeaker, ebx }
    __asm { jmp HookSubmitSourceBuffer }
}
// Énergie de chaque canal de chaque voix (flottants entrelacés), journalisée toutes les 5 s : montre par quelle voix et
// dans quel canal passent les sons.
HRESULT STDMETHODCALLTYPE HookSubmitSourceBuffer(void* voice, const void* buffer, const void* wma) {
    // Étape 1 de la stéréo (docs) : adresse, dans le moteur, du code qui envoie le flux 5.1, pour remonter à son mélangeur.
    static bool s_callerLogged = false;
    if (voice == g_surroundVoice && !s_callerLogged) {
        s_callerLogged = true;
        const auto caller = reinterpret_cast<std::uintptr_t>(_ReturnAddress());
        HMODULE module = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(caller), &module);
        log::Info("Audio : flux 5.1 envoyé depuis 0x{:08X} (module 0x{:08X}, décalage 0x{:X})", caller,
                  reinterpret_cast<std::uintptr_t>(module), caller - reinterpret_cast<std::uintptr_t>(module));
        if (caller == 0x1084A298 && g_engineSpeaker != nullptr) {
            void** vtable = *static_cast<void***>(g_engineSpeaker);
            log::Info("Audio : objet de sortie du moteur 0x{:08X}, vtable 0x{:08X}, méthode +0x108 = 0x{:08X}, +0x124 (conversion 5.1) = 0x{:08X}",
                      reinterpret_cast<std::uintptr_t>(g_engineSpeaker), reinterpret_cast<std::uintptr_t>(vtable),
                      reinterpret_cast<std::uintptr_t>(vtable[0x108 / 4]), reinterpret_cast<std::uintptr_t>(vtable[0x124 / 4]));
        }
    }
    // Correction du flux 5.1 du moteur. En sortie stéréo, la voix a été créée en stéréo (voir HookCreateSourceVoice) :
    // les 6 canaux sont convertis ici en 2, sur place, et XAudio2 reçoit une copie du descripteur avec la taille réduite.
    XAudio2BufferFull converted{};
    // Ressemblance entre les 6 canaux d'origine (corrélation normalisée), journalisée 6 fois : révèle les paires stéréo
    // et les canaux qui portent le même son, pour connaître le rôle de chaque canal du moteur.
    if (voice == g_surroundVoice && buffer != nullptr && g_correlationLogs < 6) {
        const auto* b = static_cast<const XAudio2Buffer*>(buffer);
        const auto* s = reinterpret_cast<const float*>(b->audioData);
        const std::uint32_t frames = s != nullptr ? b->audioBytes / (6 * sizeof(float)) : 0;
        for (std::uint32_t i = 0; i < frames; ++i) {
            for (int a = 0; a < 6; ++a) {
                for (int c = 0; c < 6; ++c) {
                    g_cross[a][c] += static_cast<double>(s[i * 6 + a]) * s[i * 6 + c];
                }
            }
        }
        const ULONGLONG now = GetTickCount64();
        if (g_nextCorrelationLog == 0) {
            g_nextCorrelationLog = now + 5000;
        } else if (now >= g_nextCorrelationLog) {
            g_nextCorrelationLog = now + 5000;
            ++g_correlationLogs;
            std::string text;
            for (int a = 0; a < 6; ++a) {
                text += std::format(" | {}:", a);
                for (int c = 0; c < 6; ++c) {
                    const double norm = std::sqrt(g_cross[a][a] * g_cross[c][c]);
                    text += std::format(" {:+.2f}", norm > 0.0 ? g_cross[a][c] / norm : 0.0);
                }
            }
            std::memset(g_cross, 0, sizeof(g_cross));
            log::Info("Audio : ressemblance entre canaux d'origine (ligne a : colonnes 0 à 5){}", text);
        }
    }
    if (voice == g_surroundVoice && buffer != nullptr && GetConfig().audioChannelFix) {
        const auto* b = static_cast<const XAudio2Buffer*>(buffer);
        if (b->audioData != nullptr) {
            auto* samples = const_cast<float*>(reinterpret_cast<const float*>(b->audioData));
            const std::uint32_t frames = b->audioBytes / (6 * sizeof(float));
            if (g_surroundAsStereo) {
                DownmixSurroundToStereo(samples, frames, true);
                std::memcpy(&converted, buffer, sizeof(converted));
                converted.audioBytes = frames * 2 * sizeof(float);
                buffer = &converted;
            } else {
                RemapSurroundFrames(samples, frames, GetConfig().audioChannelOrder);
            }
        }
    }    if (buffer != nullptr && g_energyLogs < 12) {
        for (VoiceMeter& meter : g_meters) {
            if (meter.voice != voice || meter.channels == 0 || meter.channels > 8) {
                continue;
            }
            const auto* b = static_cast<const XAudio2Buffer*>(buffer);
            const auto* samples = reinterpret_cast<const float*>(b->audioData);
            const UINT32 frames = b->audioBytes / (meter.channels * sizeof(float));
            for (UINT32 i = 0; samples != nullptr && i < frames; ++i) {
                for (UINT32 c = 0; c < meter.channels; ++c) {
                    const double s = samples[i * meter.channels + c];
                    meter.energy[c] += s * s;
                }
            }
            meter.frames += frames;
        }
        const ULONGLONG now = GetTickCount64();
        if (g_nextEnergyLog == 0) {
            g_nextEnergyLog = now + 5000;
        } else if (now >= g_nextEnergyLog) {
            g_nextEnergyLog = now + 5000;
            ++g_energyLogs;
            std::string text;
            for (VoiceMeter& meter : g_meters) {
                if (meter.voice == nullptr) {
                    continue;
                }
                text += std::format(" | {} canaux :", meter.channels);
                for (UINT32 c = 0; c < meter.channels; ++c) {
                    text += std::format(" {:.4f}", meter.frames > 0 ? std::sqrt(meter.energy[c] / meter.frames) : 0.0);
                    meter.energy[c] = 0.0;
                }
                meter.frames = 0;
            }
            log::Info("Audio : niveau par voix et par canal (5.1 dans l'ordre AvG AvD C Basses ArG ArD){}", text);
        }
    }
    return g_submitSourceBuffer(voice, buffer, wma);
}

HRESULT STDMETHODCALLTYPE HookCreateSourceVoice(void* xaudio, void** voice, const WAVEFORMATEX* format, UINT32 flags,
                                                float maxRatio, void* callback, const void* sends,
                                                const void* effects) {
    // Sortie stéréo : la voix 5.1 du moteur est créée en stéréo (flottants 32 bits, même fréquence) ; la conversion des
    // canaux est faite par le mod dans HookSubmitSourceBuffer. Le moteur continue de croire qu'il a une voix 5.1.
    WAVEFORMATEX stereo{};
    const bool asStereo = format != nullptr && format->nChannels == 6 && g_masterChannels == 2 &&
                          GetConfig().audioChannelFix;
    if (asStereo) {
        stereo.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        stereo.nChannels = 2;
        stereo.nSamplesPerSec = format->nSamplesPerSec;
        stereo.wBitsPerSample = 32;
        stereo.nBlockAlign = 2 * sizeof(float);
        stereo.nAvgBytesPerSec = stereo.nSamplesPerSec * stereo.nBlockAlign;
    }
    const HRESULT result = g_createSourceVoice(xaudio, voice, asStereo ? &stereo : format, flags, maxRatio, callback,
                                               sends, effects);
    if (format != nullptr && g_sourcesLogged < 20) {
        ++g_sourcesLogged;
        log::Info("Audio : voix source {} : {} canal(aux), {} Hz, format 0x{:X}, envois {}",
                  SUCCEEDED(result) && voice != nullptr ? *voice : nullptr, format->nChannels, format->nSamplesPerSec,
                  format->wFormatTag, sends != nullptr ? "définis" : "par défaut");
    }
    if (SUCCEEDED(result) && voice != nullptr && *voice != nullptr && format != nullptr) {
        TrackVoice(*voice, asStereo ? 2 : format->nChannels);
        if (g_submitSourceBuffer == nullptr) {
            hooking::PatchVtable(*voice, kSubmitSourceBufferIndex, reinterpret_cast<SubmitSourceBufferFn>(&SubmitSourceBufferRelay),
                                 &g_submitSourceBuffer);
        }
    }
    if (SUCCEEDED(result) && voice != nullptr && *voice != nullptr && format != nullptr && format->nChannels == 6) {
        g_surroundVoice = *voice;
        g_surroundAsStereo = asStereo;
        if (asStereo) {
            log::Info("Audio : voix 5.1 du moteur créée en stéréo, conversion des canaux faite par le mod");
        } else if (GetConfig().audioChannelFix) {
            const int* o = GetConfig().audioChannelOrder;
            log::Info("Audio : canaux du flux 5.1 réordonnés dans les échantillons (AvG AvD C Basses ArG ArD <- {},{},{},{},{},{})",
                      o[0], o[1], o[2], o[3], o[4], o[5]);
        }
        if (format->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
            log::Info("Audio : masque de canaux du flux 6 canaux : 0x{:X}",
                      reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(format)->dwChannelMask);
        }
    }
    if (SUCCEEDED(result) && voice != nullptr && *voice != nullptr && g_setOutputMatrix == nullptr) {
        hooking::PatchVtable(*voice, kSetOutputMatrixIndex, &HookSetOutputMatrix, &g_setOutputMatrix);
    }
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateSubmixVoice(void* xaudio, void** voice, UINT32 channels, UINT32 rate, UINT32 flags,
                                                UINT32 stage, const void* sends, const void* effects) {
    const HRESULT result = g_createSubmixVoice(xaudio, voice, channels, rate, flags, stage, sends, effects);
    log::Info("Audio : voix de mixage {} : {} canaux, {} Hz, étape {}",
              SUCCEEDED(result) && voice != nullptr ? *voice : nullptr, channels, rate, stage);
    return result;
}

std::string GuidText(REFGUID guid) {
    return std::format("{:08X}-{:04X}-{:04X}-{:02X}{:02X}-{:02X}{:02X}{:02X}{:02X}{:02X}{:02X}", guid.Data1, guid.Data2,
                       guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4],
                       guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

HRESULT STDMETHODCALLTYPE HookCreateMasteringVoice(void* xaudio, void** voice, UINT32 channels, UINT32 rate,
                                                   UINT32 flags, UINT32 device, const void* effects) {
    // [Audio] Channels : sortie forcée en 5.1 ou 7.1. Avec un casque (2 canaux), le moteur envoie ses sons mono à droite
    // seulement (constaté le 10/10/2026) ; XAudio2 ramène ensuite les canaux forcés vers ceux du périphérique.
    const UINT32 requested = channels;
    if (channels == 0 && GetConfig().audioChannels != 0) {
        channels = static_cast<UINT32>(GetConfig().audioChannels);
    }
    const HRESULT result = g_createMasteringVoice(xaudio, voice, channels, rate, flags, device, effects);
    // Canaux réellement obtenus : IXAudio2Voice::GetVoiceDetails (index 0 de la vtable, pas d'IUnknown), structure
    // XAUDIO2_VOICE_DETAILS { CreationFlags, InputChannels, InputSampleRate }.
    UINT32 details[4]{};
    if (SUCCEEDED(result) && voice != nullptr && *voice != nullptr) {
        using GetVoiceDetailsFn = void(STDMETHODCALLTYPE*)(void*, UINT32*);
        const auto getDetails = reinterpret_cast<GetVoiceDetailsFn>((*reinterpret_cast<void***>(*voice))[0]);
        getDetails(*voice, details);
        if (details[1] != 0) {
            g_masterChannels = details[1];
        }
    }
    log::Info("Audio : XAudio2 voix maîtresse : canaux demandés par le jeu {} (0 = ceux de Windows), obtenus {}, {} Hz -> "
              "0x{:08X}",
              requested, details[1], details[2], static_cast<unsigned long>(result));
    return result;
}

HRESULT STDMETHODCALLTYPE HookCreateSoundBuffer(void* dsound, const void* desc, void** buffer, void* outer) {
    const HRESULT result = g_createSoundBuffer(dsound, desc, buffer, outer);
    if (desc != nullptr && g_buffersLogged < 12) {
        ++g_buffersLogged;
        const auto* fields = static_cast<const DWORD*>(desc);
        const auto* format = *reinterpret_cast<const WAVEFORMATEX* const*>(fields + 4);
        if (format != nullptr) {
            log::Info("Audio : DirectSound CreateSoundBuffer options 0x{:X}, {} canaux, {} Hz, {} bits, format 0x{:X}",
                      fields[1], format->nChannels, format->nSamplesPerSec, format->wBitsPerSample,
                      format->wFormatTag);
        } else {
            log::Info("Audio : DirectSound CreateSoundBuffer options 0x{:X} (tampon primaire, sans format)", fields[1]);
        }
    }
    return result;
}

HRESULT WINAPI HookCoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD context, REFIID iid, LPVOID* object) {
    const HRESULT result = g_coCreateInstance(clsid, outer, context, iid, object);
    if (FAILED(result) || object == nullptr || *object == nullptr) {
        return result;
    }
    if (IsEqualGUID(clsid, kClsidDirectSound8) || IsEqualGUID(clsid, kClsidDirectSound)) {
        log::Info("Audio : le moteur crée DirectSound ({})", GuidText(clsid));
        if (g_createSoundBuffer == nullptr) {
            hooking::PatchVtable(*object, kCreateSoundBufferIndex, &HookCreateSoundBuffer, &g_createSoundBuffer);
        }
    } else if (clsid.Data2 == 0xEB21 || clsid.Data1 == 0x5A508685 || clsid.Data1 == 0xFAC23F48 ||
               clsid.Data1 == 0xE21A7345) {
        // CLSID de XAudio2 2.x (plusieurs versions) : relevé de la classe, puis interception de la voix maîtresse.
        log::Info("Audio : le moteur crée XAudio2 ({})", GuidText(clsid));
        if (g_createMasteringVoice == nullptr) {
            hooking::PatchVtable(*object, kCreateMasteringVoiceIndex, &HookCreateMasteringVoice,
                                 &g_createMasteringVoice);
            hooking::PatchVtable(*object, kCreateSourceVoiceIndex, &HookCreateSourceVoice, &g_createSourceVoice);
            hooking::PatchVtable(*object, kCreateSubmixVoiceIndex, &HookCreateSubmixVoice, &g_createSubmixVoice);
        }
    } else {
        log::Info("Audio : CoCreateInstance {} par le moteur", GuidText(clsid));
    }
    return result;
}

}  // namespace

bool ParseChannelOrder(const char* text, int order[6]) noexcept {
    int parsed[6]{};
    int count = 0;
    bool used[6]{};
    const char* p = text;
    while (p != nullptr && *p != '\0' && count < 6) {
        while (*p == ' ' || *p == ',') {
            ++p;
        }
        if (*p < '0' || *p > '5') {
            return false;
        }
        const int value = *p - '0';
        if (used[value]) {
            return false;  // chaque canal du moteur une seule fois
        }
        used[value] = true;
        parsed[count++] = value;
        ++p;
        while (*p == ' ') {
            ++p;
        }
        if (*p != ',' && *p != '\0') {
            return false;
        }
    }
    if (count != 6) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        order[i] = parsed[i];
    }
    return true;
}

void RemapSurroundFrames(float* samples, std::uint32_t frames, const int order[6]) noexcept {
    if (samples == nullptr) {
        return;
    }
    for (std::uint32_t i = 0; i < frames; ++i) {
        float* frame = samples + static_cast<std::size_t>(i) * 6;
        const float source[6] = {frame[0], frame[1], frame[2], frame[3], frame[4], frame[5]};
        for (int c = 0; c < 6; ++c) {
            frame[c] = source[order[c]];
        }
    }
}
void DownmixSurroundToStereo(float* samples, std::uint32_t frames, bool standard) noexcept {
    if (samples == nullptr) {
        return;
    }
    constexpr float kShared = 0.7071f;
    constexpr float kLfe = 0.5f;
    for (std::uint32_t i = 0; i < frames; ++i) {
        const float* f = samples + static_cast<std::size_t>(i) * 6;
        // Flux du moteur dans l'ordre de Windows (AvG AvD C Basses ArG ArD), mais placement des sons faussé : avant droit
        // et arrière gauche remplis, avant gauche et arrière droit presque vides (mesures du 10/10/2026 : niveaux et
        // ressemblance entre canaux, piano seul au centre, basses liées au centre). Tous les canaux sont donc envoyés
        // à égalité des deux côtés : plus aucun son d'un seul côté, au prix de la spatialisation.
        float left = 0.0f;
        float right = 0.0f;
        if (standard) {
            // Mélange 5.1 vers stéréo habituel (ordre de Windows : AvG AvD C Basses ArG ArD). C'est la conversion de XAudio2 2.1,
    // pas le placement du moteur, qui envoyait les sons d'un seul côté (vérifié à l'écoute le 13/10/2026).
            left = f[0] + kShared * f[2] + kShared * f[4] + kLfe * f[3];
            right = f[1] + kShared * f[2] + kShared * f[5] + kLfe * f[3];
        } else {
            left = right = kShared * (f[0] + f[1] + f[2] + f[4] + f[5]) + kLfe * f[3];
        }
        samples[static_cast<std::size_t>(i) * 2] = left;
        samples[static_cast<std::size_t>(i) * 2 + 1] = right;
    }
}
void InstallAudioProbe(HMODULE engineModule) {
    if (!hooking::PatchImport(engineModule, "ole32.dll", "CoCreateInstance", &HookCoCreateInstance,
                              &g_coCreateInstance)) {
        log::Warn("Audio : CoCreateInstance du moteur non intercepté (diagnostic audio indisponible)");
    }
}

}  // namespace hca::audio
