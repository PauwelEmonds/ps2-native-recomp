#pragma once

// On-screen controls for phones and tablets: a PS2 controller drawn over the picture (the same model as OpenTricky's touch_sdl.c,
// with the DualShock 2's buttons). A stick appears where the left thumb lands (it also presses the d-pad when pushed far, for menus
// that read only the d-pad), triangle circle cross square in their colours on the right, L1 L2 and R1 R2 in the top corners, SELECT
// and START at the top, and a button beside them that opens the quick menu (save, load, look). On Android always; elsewhere with
// PS2_TOUCH=1, where the left mouse button stands in for a finger. The controls hide while a real controller is in use and come back
// at the next touch.

#include <SDL.h>
#include <cstdint>

namespace touch {

bool enabled();
// Every window event while the game has the input (not the quick menu); w, h: the window in pixels. True when it was the controls'.
bool event(const SDL_Event &e, int w, int h);
// What the controls press on the first controller: the PS2 button bits (window.cpp's Button, pressed = 1) and the left stick.
// The stick is 0x80, 0x80 at rest.
void pad(std::uint16_t &pressed, std::uint8_t &lx, std::uint8_t &ly);
// True once after the options button was let go (the caller opens the quick menu). Every finger is let go then.
bool take_options();
// Every finger let go (the app went to the background, the quick menu opened).
void release_all();
// The shapes for a window of w x h pixels to the present pass, when they changed. `hidden`: the quick menu is open.
void draw(int w, int h, bool hidden);

}  // namespace touch
