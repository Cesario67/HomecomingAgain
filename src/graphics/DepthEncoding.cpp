#include "graphics/DepthEncoding.h"

#include "core/Log.h"

namespace hca::graphics::depth {

namespace {

Encoding g_encoding;
bool g_logged = false;

}  // namespace

bool ParseEncoding(UINT startRegister, const float* values, UINT vectorCount, Encoding& encoding) noexcept {
    if (values == nullptr || startRegister > kCoefficientsRegister ||
        startRegister + vectorCount <= kCoefficientsRegister) {
        return false;
    }
    const float* c = values + (kCoefficientsRegister - startRegister) * 4;
    // Un facteur nul ou négatif ne peut pas être ce codage : ignoré.
    if (!(c[0] > 0.01f && c[0] <= 1.0f && c[1] >= 0.0f && c[1] < 1.0f)) {
        return false;
    }
    encoding.scale = c[0];
    encoding.bias = c[1];
    return true;
}

void OnPixelShaderConstants(UINT startRegister, const float* values, UINT vectorCount) noexcept {
    Encoding encoding;
    if (!ParseEncoding(startRegister, values, vectorCount, encoding)) {
        return;
    }
    g_encoding = encoding;
    if (!g_logged) {
        g_logged = true;
        log::Info("Profondeur : codage du moteur lu (z/w x {:.3f} + {:.3f})", encoding.scale, encoding.bias);
    }
}

Encoding Current() noexcept {
    return g_encoding;
}

}  // namespace hca::graphics::depth
