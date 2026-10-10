#include "graphics/BackBufferDump.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Strings.h"

namespace hca::graphics::dump {

namespace {

constexpr ULONGLONG kIntervalMs = 15000;
constexpr int kMaxDumps = 8;

ULONGLONG g_nextDump = 0;
int g_dumpsLeft = kMaxDumps;

template <typename T>
void SafeRelease(T*& object) noexcept {
    if (object != nullptr) {
        object->Release();
        object = nullptr;
    }
}

// BMP 24 bits, lignes de bas en haut, chaque ligne alignée sur 4 octets.
void WriteBmp(const std::filesystem::path& path, const BYTE* pixels, INT pitch, UINT width, UINT height) {
    const std::uint32_t rowSize = (width * 3 + 3) & ~3u;
    const std::uint32_t imageSize = rowSize * height;
    BITMAPFILEHEADER file{};
    file.bfType = 0x4D42;  // « BM »
    file.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + imageSize;
    BITMAPINFOHEADER info{};
    info.biSize = sizeof(info);
    info.biWidth = static_cast<LONG>(width);
    info.biHeight = static_cast<LONG>(height);
    info.biPlanes = 1;
    info.biBitCount = 24;
    info.biSizeImage = imageSize;
    std::vector<BYTE> row(rowSize, 0);
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
    out.write(reinterpret_cast<const char*>(&info), sizeof(info));
    for (UINT y = height; y-- > 0;) {
        const BYTE* source = pixels + static_cast<std::ptrdiff_t>(y) * pitch;
        for (UINT x = 0; x < width; ++x) {
            row[x * 3 + 0] = source[x * 4 + 0];  // B
            row[x * 3 + 1] = source[x * 4 + 1];  // G
            row[x * 3 + 2] = source[x * 4 + 2];  // R
        }
        out.write(reinterpret_cast<const char*>(row.data()), rowSize);
    }
}

}  // namespace

void OnPresent(IDirect3DDevice9* device) noexcept {
    const ULONGLONG now = GetTickCount64();
    if (g_dumpsLeft <= 0) {
        return;
    }
    if (g_nextDump == 0) {
        g_nextDump = now + kIntervalMs;
        return;
    }
    if (now < g_nextDump) {
        return;
    }
    g_nextDump = now + kIntervalMs;
    --g_dumpsLeft;

    IDirect3DSurface9* backBuffer = nullptr;
    IDirect3DSurface9* copy = nullptr;
    try {
        D3DSURFACE_DESC desc{};
        HRESULT result = device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backBuffer);
        if (SUCCEEDED(result)) {
            backBuffer->GetDesc(&desc);
            result = desc.Format == D3DFMT_A8R8G8B8 || desc.Format == D3DFMT_X8R8G8B8 ? S_OK : E_NOTIMPL;
        }
        if (SUCCEEDED(result)) {
            result = device->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &copy,
                                                         nullptr);
        }
        if (SUCCEEDED(result)) {
            result = device->GetRenderTargetData(backBuffer, copy);
        }
        D3DLOCKED_RECT rect{};
        if (SUCCEEDED(result)) {
            result = copy->LockRect(&rect, nullptr, D3DLOCK_READONLY);
        }
        if (FAILED(result)) {
            log::Warn("Copie du tampon arrière impossible (0x{:08X}, format {})", static_cast<unsigned long>(result),
                      static_cast<int>(desc.Format));
        } else {
            // Moyenne de luminosité sur une grille : 0 = image noire, sans avoir à ouvrir le fichier.
            const auto* pixels = static_cast<const BYTE*>(rect.pBits);
            std::uint64_t sum = 0;
            std::uint32_t samples = 0;
            for (UINT y = 0; y < desc.Height; y += 8) {
                for (UINT x = 0; x < desc.Width; x += 8) {
                    const BYTE* p = pixels + static_cast<std::ptrdiff_t>(y) * rect.Pitch + x * 4;
                    sum += (p[0] + p[1] + p[2]) / 3;
                    ++samples;
                }
            }
            const std::filesystem::path path = std::filesystem::path(GetPaths().modDir) / L"captures" /
                                               (L"backbuffer_" + FileTimestamp() + L".bmp");
            std::error_code error;
            std::filesystem::create_directories(path.parent_path(), error);
            WriteBmp(path, pixels, rect.Pitch, desc.Width, desc.Height);
            copy->UnlockRect();
            log::Info("Tampon arrière enregistré ({}x{}, luminosité moyenne {} sur 255) : {}", desc.Width, desc.Height,
                      samples > 0 ? sum / samples : 0, WideToUtf8(path.wstring()));
        }
    } catch (...) {
        log::Warn("Copie du tampon arrière : exception");
    }
    SafeRelease(copy);
    SafeRelease(backBuffer);
}

}  // namespace hca::graphics::dump
