#include "audio/AudioProbe.h"

#include <mmreg.h>
#include <objbase.h>

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
