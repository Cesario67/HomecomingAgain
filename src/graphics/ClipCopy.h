#pragma once

#include <d3d9.h>

namespace hca::graphics::clipcopy {

// Black screen in every resolution that is not 16:9 (reported 27/10/2026: 1920x810, 2560x1080, 3840x1080 on a
// 1920x1080 screen). Verified by F11 captures and by logging the result and the rectangles of the engine's calls: the
// last call of each frame is a StretchRect from the engine's 1920x1080 scene target to the back buffer (1920x810),
// with a destination rectangle (0,-135,1920,945) that leaves the surface. Direct3D 9 (DXVK here) answers 0x8876086C
// (D3DERR_INVALIDCALL), so the back buffer stays black. With a 16:9 back buffer the rectangle fits and it works.
// The engine therefore shows its 16:9 picture centred at full size, cropped at the top and bottom, not squashed.
//
// Fix: when that call fails, retry it with the destination rectangle clipped to the surface and the source rectangle
// moved by the same proportion (ClipToSurface). Disabled by `[Graphics] ClipFinalCopy=0`.
void Install(IDirect3DDevice9* device);

}  // namespace hca::graphics::clipcopy
