#include "graphics/D3D9On12.h"

#include <d3d9on12.h>

#include "core/Log.h"

namespace hca::graphics::on12 {

IDirect3D9* Create(UINT sdkVersion) noexcept {
    const HMODULE d3d9 = GetModuleHandleW(L"d3d9.dll");
    const auto create = d3d9 != nullptr ? reinterpret_cast<PFN_Direct3DCreate9On12>(
                                              GetProcAddress(d3d9, "Direct3DCreate9On12"))
                                        : nullptr;
    if (create == nullptr) {
        log::Warn("D3D9On12 : Direct3DCreate9On12 absent de d3d9.dll, Direct3D 9 habituel utilisé");
        return nullptr;
    }
    // Sans périphérique Direct3D 12 fourni, la couche crée le sien sur l'adaptateur choisi par CreateDevice.
    D3D9ON12_ARGS args{};
    args.Enable9On12 = TRUE;
    IDirect3D9* direct3d = create(sdkVersion, &args, 1);
    if (direct3d == nullptr) {
        log::Warn("D3D9On12 : création impossible, Direct3D 9 habituel utilisé");
        return nullptr;
    }
    log::Info("D3D9On12 actif : Direct3D 9 du jeu exécuté par Direct3D 12 (expérimental)");
    return direct3d;
}

void LogDevice(IDirect3DDevice9* device) noexcept {
    if (device == nullptr) {
        return;
    }
    IDirect3DDevice9On12* on12 = nullptr;
    if (SUCCEEDED(device->QueryInterface(__uuidof(IDirect3DDevice9On12), reinterpret_cast<void**>(&on12))) &&
        on12 != nullptr) {
        log::Info("D3D9On12 : périphérique confirmé (IDirect3DDevice9On12 disponible)");
        on12->Release();
    } else {
        log::Warn("D3D9On12 : le périphérique n'expose pas IDirect3DDevice9On12");
    }
}

}  // namespace hca::graphics::on12
