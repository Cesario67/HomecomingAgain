#pragma once

#include <d3d9.h>

namespace hca::graphics::moviebars {

// Pre-rendered videos (Bink, 1280x720) stretched across a back buffer wider than 16:9 (reported 10/11/2026 on a 32:9
// target, 1920x540). F11 capture of a cinematic: the engine draws one full-screen triangle strip with a YUV to RGB
// pixel shader, reading three L8 textures (1280x720 and two 640x360), straight into the scene target, right after
// a black Clear of that target. With Hor+ the target is as wide as the screen, so the picture covers all of it.
//
// Fix: for that draw only, the viewport and the scissor rectangle are set to the centred 16:9 rectangle of the
// target; the Clear already left the rest black. Restored right after the draw. Videos drawn into a texture first
// (the title screen loop, drawn into a 1280x720 target then as an interface quad) are not handled yet.
// Disabled by `[Graphics] MovieBars=0`.
void Install(IDirect3DDevice9* device);

}  // namespace hca::graphics::moviebars
