#include "graphics/Projection.h"

namespace hca::graphics {

namespace {

bool IsZero(float value) noexcept {
    return value == 0.0f;
}

}  // namespace

bool ParseProjection(const float* r, Projection& projection) noexcept {
    if (r == nullptr) {
        return false;
    }
    const bool shape = r[0] > 0.0f && IsZero(r[1]) && IsZero(r[2]) && IsZero(r[3]) &&  //
                       IsZero(r[4]) && r[5] > 0.0f && IsZero(r[6]) && IsZero(r[7]) &&   //
                       IsZero(r[8]) && IsZero(r[9]) && r[10] > 0.0f && r[11] < 0.0f &&  //
                       IsZero(r[12]) && IsZero(r[13]) && r[14] == 1.0f && IsZero(r[15]);
    if (!shape) {
        return false;
    }
    projection = {r[0], r[5], r[10], r[11]};
    return true;
}

}  // namespace hca::graphics
