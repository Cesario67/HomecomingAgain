#include "audio/AudioProbe.h"

#include <mmreg.h>
#include <objbase.h>

#include <cmath>
#include <cstdint>
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
double g_channelEnergy[8]{};
std::uint64_t g_frames = 0;
ULONGLONG g_nextEnergyLog = 0;
int g_energyLogs = 0;

struct XAudio2Buffer {
    UINT32 flags;
    UINT32 audioBytes;
    const BYTE* audioData;
};

// Énergie de chaque canal du flux 6 canaux (flottants entrelacés), journalisée toutes les 5 s : montre dans quel canal
// le moteur écrit ses sons.
HRESULT STDMETHODCALLTYPE HookSubmitSourceBuffer(void* voice, const void* buffer, const void* wma) {
    if (voice == g_surroundVoice && buffer != nullptr && g_energyLogs < 12) {
        const auto* b = static_cast<const XAudio2Buffer*>(buffer);
        const auto* samples = reinterpret_cast<const float*>(b->audioData);
        const UINT32 frames = b->audioBytes / (6 * sizeof(float));
        for (UINT32 i = 0; samples != nullptr && i < frames; ++i) {
            for (int c = 0; c < 6; ++c) {
                const double s = samples[i * 6 + c];
                g_channelEnergy[c] += s * s;
            }
        }
        g_frames += frames;
        const ULONGLONG now = GetTickCount64();
        if (g_nextEnergyLog == 0) {
            g_nextEnergyLog = now + 5000;
        } else if (now >= g_nextEnergyLog && g_frames > 0) {
            g_nextEnergyLog = now + 5000;
            ++g_energyLogs;
            std::string text;
            for (int c = 0; c < 6; ++c) {
                text += std::format(" {:.4f}", std::sqrt(g_channelEnergy[c] / static_cast<double>(g_frames)));
                g_channelEnergy[c] = 0.0;
            }
            g_frames = 0;
            log::Info("Audio : niveau des canaux du flux 6 canaux (ordre de Windows : AvG AvD Centre Basses ArG ArD) :{}",
                      text);
        }
    }
    return g_submitSourceBuffer(voice, buffer, wma);
}

HRESULT STDMETHODCALLTYPE HookCreateSourceVoice(void* xaudio, void** voice, const WAVEFORMATEX* format, UINT32 flags,
                                                float maxRatio, void* callback, const void* sends,
                                                const void* effects) {
    const HRESULT result = g_createSourceVoice(xaudio, voice, format, flags, maxRatio, callback, sends, effects);
    if (format != nullptr && g_sourcesLogged < 20) {
        ++g_sourcesLogged;
        log::Info("Audio : voix source {} : {} canal(aux), {} Hz, format 0x{:X}, envois {}",
                  SUCCEEDED(result) && voice != nullptr ? *voice : nullptr, format->nChannels, format->nSamplesPerSec,
                  format->wFormatTag, sends != nullptr ? "définis" : "par défaut");
    }
    if (SUCCEEDED(result) && voice != nullptr && *voice != nullptr && format != nullptr && format->nChannels == 6) {
        g_surroundVoice = *voice;
        if (g_submitSourceBuffer == nullptr) {
            hooking::PatchVtable(*voice, kSubmitSourceBufferIndex, &HookSubmitSourceBuffer, &g_submitSourceBuffer);
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

void InstallAudioProbe(HMODULE engineModule) {
    if (!hooking::PatchImport(engineModule, "ole32.dll", "CoCreateInstance", &HookCoCreateInstance,
                              &g_coCreateInstance)) {
        log::Warn("Audio : CoCreateInstance du moteur non intercepté (diagnostic audio indisponible)");
    }
}

}  // namespace hca::audio
