#pragma once

// A picture laid over the game's picture in the window: the quick menu of the native runner (host/quickmenu.cpp). It is drawn in the
// present pass only, after the game's picture, so the game, the pictures it saves (--frames-dir, F9) and its save states never contain it.
// Called on the thread that presents (the window thread in a run with threads, the emulation thread in a run with one).

#include <cstdint>

namespace ps2::gs::gpu {

// rgba: straight alpha, red in the low byte, `width` x `height`, its top left corner at x, y in window pixels. nullptr takes it away.
void set_overlay(const std::uint32_t *rgba, std::uint32_t width, std::uint32_t height, std::uint32_t x, std::uint32_t y);
// The on-screen touch controls (host/touch.cpp), drawn in the present pass after the game's picture and before the quick menu's panel,
// as distance fields (sharp at any screen density). One shape: a rounded box (mode 0: a circle when the radius is half its size, a
// pill when it is half its height) with a fill lit a little from above, a rim and a soft edge (a shadow), or a glyph (mode 1): the
// line segments segments[first .. first + count) as round-ended strokes. Colours are straight rgba. Laid out as the shader reads it.
struct TouchShape {
    float rect[4];             // x0, y0, x1, y1 in window pixels (a glyph: its box; the segments are in pixels from the box's centre)
    float fill[4];             // the fill (a glyph: the stroke colour)
    float line[4];             // the rim
    float rim = 0, radius = 0, soft = 0, stroke = 0;  // rim width, corner radius, edge blur, a glyph's stroke width (pixels)
    std::uint32_t mode = 0, first = 0, count = 0, unused = 0;
};
static_assert(sizeof(TouchShape) == 80, "the shader's Shape");
// segments: 4 floats each (x0, y0, x1, y1). count 0 takes the controls away. The window's size in pixels places them.
void set_touch_shapes(const TouchShape *shapes, std::uint32_t count, const float *segments, std::uint32_t segment_count, std::uint32_t window_width,
                      std::uint32_t window_height);

// The last picture of the game once more, with the picture look of now and the overlay: for a game that waits (the menu is open) and
// sends no new pictures. False when there is no window surface or no picture yet.
bool present_again();

}  // namespace ps2::gs::gpu
