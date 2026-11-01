#pragma once

#include <d3d9.h>

namespace hca::graphics::horplus {

// PROTOTYPE (27/10/2026). Hor+ for back buffers wider than 16:9. Without it the engine draws a 16:9 picture sized from
// the width and crops the top and bottom: an ultrawide screen loses height instead of gaining width (verified by F11
// captures, see docs/a-faire.md).
//
// The three changes, all at the Direct3D 9 level (hypothesis of design, validated only by in-game tests):
//  - render targets and depth surfaces that are exact 16:9 pictures are created at the back buffer's ratio;
//  - viewports and scissor rectangles of that size follow;
//  - a projection made for 16:9 (c0 x4 sent to a vertex shader) gets its x row multiplied by (16/9) / (width / height):
//    the vertical field of view is kept and the horizontal one grows.
// Full-screen quads (post-processing) are in clip space and need nothing. Disabled by `[Graphics] HorPlus=0`.
void Install(IDirect3DDevice9* device);

// Back buffer size requested by the game (CreateDevice and Reset). Must be known before the targets are created.
void SetBackBufferSize(unsigned width, unsigned height) noexcept;

// The map and the examined objects are world cameras drawn in the fixed 1280x720 interface area, handled by UiClip.
// Factor for the x row of their projection when only the Direct3D part of Hor+ runs (1 with the engine redirect, which
// already scales it: measured 1/11/2026, the redirected map is 1037 px wide at 3840x1080, as at 1920x1080).
float LegacyAreaXFactor() noexcept;
// Height the engine believes the target has in that area (see HorPlusRules.h).
long LegacyAreaBeliefHeight(long targetWidth, long targetHeight) noexcept;

}  // namespace hca::graphics::horplus
