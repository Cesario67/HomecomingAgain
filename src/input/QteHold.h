#pragma once

#include <windows.h>

namespace hca::input {

// "Hold to mash" accessibility option ([Input] QteHold=1): during a button-mashing prompt (QTE, "struggle"), holding
// the button counts as repeated presses (input/QteHoldLogic.h).
//
// The prompt is detected through the engine's struggle prompt UI object (class with vtable 0x1106EE6C), g_SilentHill.sgl
// Steam build 6.30. Sites, original bytes verified by disassembly and against the file on 24/10/2026:
// - 0x1092E870 (56 57 8B F1 E8 rel32): starts the prompt: sets [this+28h] and the flag byte [this+30h] = 1; it
//   caches the object in this module. Hypothesis, to confirm in game: it runs once per QTE;
// - 0x1092E7E0 (6A FF 68 imm32): destructor of the object, clears the cached pointer.
// The prompt counts as active while the cached object exists and its byte [this+30h] is non-zero (hypothesis: the
// destroy paths at 0x1092E780 and 0x1092E930 clear it, as seen in the disassembly).
// Nothing is applied if the bytes differ (other version of the game).
void InstallQteHold(HMODULE engine);

// Called by the XInput hook for the first pad: the buttons the engine should see.
WORD FilterQteButtons(WORD buttons) noexcept;

}  // namespace hca::input
