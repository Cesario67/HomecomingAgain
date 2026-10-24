#pragma once

#include <windows.h>

namespace hca::input {

// "Hold to mash" accessibility option ([Input] QteHold=1): during a button-mashing prompt (QTE, "struggle"), holding
// the button counts as repeated presses (input/QteHoldLogic.h).
//
// The prompt is detected through two UI handlers that look up "StrugglePromptScript", g_SilentHill.sgl Steam build
// 6.30. Sites, original bytes verified by disassembly and against the file on 24/10/2026; behavior verified in game
// the same day (first QTE of the game, log: show with argument 5, hide about 7 s later):
// - 0x1094E3F0 (6A FF 68 25 53 E7 10), thiscall with one stack argument: shows the prompt (reaches
//   StrugglePromptActivate through 0x1092E9D0);
// - 0x1094E4A0 (6A FF 68 37 53 E7 10), thiscall: hides it (reaches StrugglePromptDeactivate through 0x1092CEC0).
// Hypothesis: a prompt that ends without going through the hide handler is covered by a 20 s timeout.
// 0x1092E870 and 0x1092E7E0, tried first, are not the QTE prompt (they run for other prompts).
// Nothing is applied if the bytes differ (other version of the game).
void InstallQteHold(HMODULE engine);

// Called by the XInput hook for the first pad: the buttons the engine should see.
WORD FilterQteButtons(WORD buttons) noexcept;

}  // namespace hca::input
