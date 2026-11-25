// On-screen controls (touch.hpp): the fingers, the PS2 buttons they hold, and the shapes the present pass draws (gpu.cpp
// set_touch_shapes). Runs on the window thread. The layout and the look follow OpenTricky's touch_sdl.c (the Xbox pad there, the
// DualShock 2 here): positions in units of the window's height, so the controls keep their size and shape on any screen.
#include "touch.hpp"

#include "ps2/os.hpp"

#include "../runtime/src/gs/gpu/overlay.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace touch {
namespace {

// the PS2 button bits (window.cpp's Button)
constexpr std::uint16_t kSelect = 1u << 0, kStart = 1u << 3, kUp = 1u << 4, kRight = 1u << 5, kDown = 1u << 6, kLeft = 1u << 7,
                        kL2 = 1u << 8, kR2 = 1u << 9, kL1 = 1u << 10, kR1 = 1u << 11, kTriangle = 1u << 12, kCircle = 1u << 13,
                        kCross = 1u << 14, kSquare = 1u << 15;

enum Anchor { kFromLeft, kFromRight, kFromCentre };
struct Button {
    const char *label;  // letters, or one of the symbols below
    bool round;         // a circle; else a pill
    float x, y, w, h;   // centre and size in window heights; x from the anchor's edge (the centre: an offset)
    Anchor anchor;
    std::uint16_t bit;  // 0: the options button
    float r, g, b;      // accent: rim, label and pressed fill
};
// symbols: \x01 triangle, \x02 circle, \x03 cross, \x04 square, \x05 the options (sliders)
const Button kButtons[] = {
    {"\x03", true, 0.21f, 0.80f, 0.135f, 0.135f, kFromRight, kCross, 0.47f, 0.64f, 1.00f},
    {"\x02", true, 0.10f, 0.69f, 0.135f, 0.135f, kFromRight, kCircle, 0.96f, 0.36f, 0.38f},
    {"\x04", true, 0.32f, 0.69f, 0.135f, 0.135f, kFromRight, kSquare, 0.93f, 0.53f, 0.80f},
    {"\x01", true, 0.21f, 0.58f, 0.135f, 0.135f, kFromRight, kTriangle, 0.25f, 0.82f, 0.62f},
    {"L2", false, 0.17f, 0.09f, 0.21f, 0.095f, kFromLeft, kL2, 0.92f, 0.93f, 0.95f},
    {"R2", false, 0.17f, 0.09f, 0.21f, 0.095f, kFromRight, kR2, 0.92f, 0.93f, 0.95f},
    {"L1", false, 0.17f, 0.21f, 0.18f, 0.085f, kFromLeft, kL1, 0.92f, 0.93f, 0.95f},
    {"R1", false, 0.17f, 0.21f, 0.18f, 0.085f, kFromRight, kR1, 0.92f, 0.93f, 0.95f},
    {"SELECT", false, -0.10f, 0.075f, 0.16f, 0.07f, kFromCentre, kSelect, 0.92f, 0.93f, 0.95f},
    {"START", false, 0.10f, 0.075f, 0.16f, 0.07f, kFromCentre, kStart, 0.92f, 0.93f, 0.95f},
    {"\x05", true, 0.27f, 0.075f, 0.075f, 0.075f, kFromCentre, 0, 0.92f, 0.93f, 0.95f},
};
constexpr int kCount = static_cast<int>(sizeof kButtons / sizeof kButtons[0]);
constexpr int kOptions = kCount - 1;

// the stick's resting place (shown faintly until a thumb lands) and reach, in window heights
constexpr float kStickX = 0.25f, kStickY = 0.70f, kStickR = 0.12f;

// Labels as strokes. Letters on a 4 x 6 grid (y down), symbols on a square from -1 to 1.
struct Glyph {
    char c;
    bool symbol;
    int n;
    float s[12][4];
};
const Glyph kGlyphs[] = {
    {'A', false, 3, {{0, 6, 2, 0}, {2, 0, 4, 6}, {0.75f, 3.9f, 3.25f, 3.9f}}},
    {'C', false, 3, {{4, 0, 0, 0}, {0, 0, 0, 6}, {0, 6, 4, 6}}},
    {'E', false, 4, {{0, 0, 0, 6}, {0, 0, 4, 0}, {0, 3, 3, 3}, {0, 6, 4, 6}}},
    {'L', false, 2, {{0, 0, 0, 6}, {0, 6, 3.6f, 6}}},
    {'R', false, 7, {{0, 6, 0, 0}, {0, 0, 2.7f, 0}, {2.7f, 0, 3.7f, 1}, {3.7f, 1, 3.7f, 2}, {3.7f, 2, 2.7f, 3}, {2.7f, 3, 0, 3}, {2.1f, 3, 3.9f, 6}}},
    {'S', false, 10, {{3.6f, 0.4f, 3, 0}, {3, 0, 1, 0}, {1, 0, 0, 1}, {0, 1, 0, 2}, {0, 2, 1, 3}, {1, 3, 3, 3}, {3, 3, 4, 4}, {4, 4, 4, 5}, {4, 5, 3, 6}, {3, 6, 0.4f, 6}}},
    {'T', false, 2, {{0, 0, 4, 0}, {2, 0, 2, 6}}},
    {'1', false, 2, {{0.8f, 1.2f, 2.2f, 0}, {2.2f, 0, 2.2f, 6}}},
    {'2', false, 5, {{0, 0, 4, 0}, {4, 0, 4, 3}, {4, 3, 0, 3}, {0, 3, 0, 6}, {0, 6, 4, 6}}},
    {'\x01', true, 3, {{0, -0.80f, 0.86f, 0.66f}, {0.86f, 0.66f, -0.86f, 0.66f}, {-0.86f, 0.66f, 0, -0.80f}}},
    {'\x02', true, 0, {}},  // a circle: made below
    {'\x03', true, 2, {{-0.74f, -0.74f, 0.74f, 0.74f}, {0.74f, -0.74f, -0.74f, 0.74f}}},
    {'\x04', true, 4, {{-0.68f, -0.68f, 0.68f, -0.68f}, {0.68f, -0.68f, 0.68f, 0.68f}, {0.68f, 0.68f, -0.68f, 0.68f}, {-0.68f, 0.68f, -0.68f, -0.68f}}},
    {'\x05', true, 6, {{-1, -0.66f, 1, -0.66f}, {-1, 0, 1, 0}, {-1, 0.66f, 1, 0.66f}, {-0.4f, -0.93f, -0.4f, -0.4f}, {0.45f, -0.27f, 0.45f, 0.27f}, {-0.1f, 0.4f, -0.1f, 0.93f}}},
};

constexpr int kMaxFingers = 10;
constexpr SDL_FingerID kMouseFinger = -2;
struct Finger {
    SDL_FingerID id = 0;
    bool used = false;
    int button = 0;  // index into kButtons, -1 the stick
} g_fingers[kMaxFingers];

float g_stick_ox = 0, g_stick_oy = 0, g_stick_x = 0, g_stick_y = 0;  // pixels: where the thumb landed, where it is
bool g_stick_on = false;
unsigned g_pressed = 0;  // bit per button
// A button stays down at least kMinHoldMs after it went down: a quick tap can be shorter than a frame, and a game does not read
// the pad every frame everywhere (THPS4's menus missed presses of 150 ms on the phone and took those of 200 ms). g_down_at: when
// each button went down; g_held: let go but still held until g_down_at + kMinHoldMs.
constexpr Uint32 kMinHoldMs = 200;
Uint32 g_down_at[16] = {};
unsigned g_held = 0;

void press(int b) {
    g_pressed |= 1u << b;
    g_held &= ~(1u << b);
    g_down_at[b] = SDL_GetTicks();
}
void let_go(int b) {
    g_pressed &= ~(1u << b);
    if (SDL_GetTicks() - g_down_at[b] < kMinHoldMs) g_held |= 1u << b;
}
// the buttons down now: pressed, or let go too soon
unsigned down_now() {
    const Uint32 now = SDL_GetTicks();
    for (int b = 0; b < 16; ++b)
        if ((g_held & (1u << b)) && now - g_down_at[b] >= kMinHoldMs) g_held &= ~(1u << b);
    return g_pressed | g_held;
}
Uint32 g_last_touch = 0, g_last_pad = 0;
int g_w = 1, g_h = 1;
bool g_options = false;
std::vector<ps2::gs::gpu::TouchShape> g_shapes, g_sent_shapes;
std::vector<float> g_segments, g_sent_segments;
int g_sent_w = 0, g_sent_h = 0;
bool g_sent_any = false;

void place(const Button &b, float &x0, float &y0, float &x1, float &y1) {
    const float H = static_cast<float>(g_h);
    float cx = b.x * H;
    const float cy = b.y * H;
    if (b.anchor == kFromCentre) cx += g_w * 0.5f;
    else if (b.anchor == kFromRight) cx = g_w - cx;
    x0 = cx - b.w * H * 0.5f;
    x1 = cx + b.w * H * 0.5f;
    y0 = cy - b.h * H * 0.5f;
    y1 = cy + b.h * H * 0.5f;
}

int hit(float px, float py) {
    const float pad = g_h * 0.02f;
    for (int i = 0; i < kCount; ++i) {
        float x0, y0, x1, y1;
        place(kButtons[i], x0, y0, x1, y1);
        if (px >= x0 - pad && px <= x1 + pad && py >= y0 - pad && py <= y1 + pad) return i;
    }
    return -1;
}

void finger_down(SDL_FingerID id, float px, float py) {
    int slot = -1;
    for (int i = 0; i < kMaxFingers; ++i)
        if (g_fingers[i].used && g_fingers[i].id == id) slot = i;
    for (int i = 0; slot < 0 && i < kMaxFingers; ++i)
        if (!g_fingers[i].used) slot = i;
    if (slot < 0) return;
    g_last_touch = SDL_GetTicks();
    const int b = hit(px, py);
    Finger &f = g_fingers[slot];
    if (b >= 0) {
        f = {id, true, b};
        press(b);
    } else if (px < g_w * 0.5f && !g_stick_on) {  // the stick, where the thumb landed
        f = {id, true, -1};
        g_stick_ox = g_stick_x = px;
        g_stick_oy = g_stick_y = py;
        g_stick_on = true;
    }
}

void finger_move(SDL_FingerID id, float px, float py) {
    for (Finger &f : g_fingers) {
        if (!f.used || f.id != id) continue;
        if (f.button < 0) {
            g_stick_x = px;
            g_stick_y = py;
        } else if (const int b = hit(px, py); b >= 0 && b != f.button) {  // sliding onto another button
            let_go(f.button);
            f.button = b;
            press(b);
        }
    }
}

void finger_up(SDL_FingerID id) {
    for (Finger &f : g_fingers) {
        if (!f.used || f.id != id) continue;
        if (f.button < 0) g_stick_on = false;
        else let_go(f.button);
        if (f.button == kOptions) g_options = true;
        f.used = false;
    }
}

// the stick's push, -1 to 1 each way (y down)
void stick(float &dx, float &dy) {
    dx = dy = 0;
    if (!g_stick_on) return;
    const float R = g_h * kStickR;
    dx = (g_stick_x - g_stick_ox) / R;
    dy = (g_stick_y - g_stick_oy) / R;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f) dx /= len, dy /= len;
}

ps2::gs::gpu::TouchShape box(float cx, float cy, float hw, float hh, float rad, float soft, const float fill[4], float rim, const float line[4]) {
    ps2::gs::gpu::TouchShape s{};
    s.rect[0] = cx - hw - soft;
    s.rect[1] = cy - hh - soft;
    s.rect[2] = cx + hw + soft;
    s.rect[3] = cy + hh + soft;
    std::memcpy(s.fill, fill, sizeof s.fill);
    std::memcpy(s.line, line, sizeof s.line);
    s.rim = rim;
    s.radius = rad;
    s.soft = soft;
    return s;
}

void shape(float cx, float cy, float hw, float hh, float rad, float soft, float fr, float fg, float fb, float fa, float rim, float lr, float lg, float lb, float la) {
    const float fill[4] = {fr, fg, fb, fa}, line[4] = {lr, lg, lb, la};
    g_shapes.push_back(box(cx, cy, hw, hh, rad, soft, fill, rim, line));
}

// one glyph: segments in pixels from (cx, cy)
void glyph(const float (*seg)[4], int n, float cx, float cy, float half_w, float half_h, float stroke, float r, float g, float b, float a) {
    ps2::gs::gpu::TouchShape s{};
    s.rect[0] = cx - half_w - stroke;
    s.rect[1] = cy - half_h - stroke;
    s.rect[2] = cx + half_w + stroke;
    s.rect[3] = cy + half_h + stroke;
    s.fill[0] = r, s.fill[1] = g, s.fill[2] = b, s.fill[3] = a;
    s.stroke = stroke;
    s.mode = 1;
    s.first = static_cast<std::uint32_t>(g_segments.size() / 4);
    s.count = static_cast<std::uint32_t>(n);
    for (int i = 0; i < n; ++i) g_segments.insert(g_segments.end(), seg[i], seg[i] + 4);
    g_shapes.push_back(s);
}

const Glyph *find_glyph(char c) {
    for (const Glyph &g : kGlyphs)
        if (g.c == c) return &g;
    return nullptr;
}

// A label centred on (cx, cy): letters h pixels high, or a symbol h pixels across.
void label(const char *text, float cx, float cy, float h, float r, float g, float b, float a) {
    if (text[0] && text[0] < 0x20) {  // a symbol
        const Glyph *gl = find_glyph(text[0]);
        if (!gl) return;
        const float k = h * 0.5f, stroke = h * 0.11f;
        float seg[16][4];
        int n = gl->n;
        if (text[0] == '\x02') {  // the circle
            n = 16;
            for (int i = 0; i < n; ++i) {
                const float a0 = 6.2831853f * i / n, a1 = 6.2831853f * (i + 1) / n;
                seg[i][0] = std::cos(a0) * 0.80f * k, seg[i][1] = std::sin(a0) * 0.80f * k;
                seg[i][2] = std::cos(a1) * 0.80f * k, seg[i][3] = std::sin(a1) * 0.80f * k;
            }
        } else {
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < 4; ++j) seg[i][j] = gl->s[i][j] * k;
        }
        glyph(seg, n, cx, cy, k, k, stroke, r, g, b, a);
        return;
    }
    const float u = h / 6.0f, stroke = h * 0.15f, adv = 5.6f * u;
    const std::size_t len = std::strlen(text);
    float x = cx - (adv * static_cast<float>(len - 1) + 4.0f * u) * 0.5f;
    for (const char *p = text; *p; ++p, x += adv) {
        const Glyph *gl = find_glyph(*p);
        if (!gl) continue;
        const float gx = x + 2.0f * u;
        float seg[12][4];
        for (int i = 0; i < gl->n; ++i) {  // grid -> pixels from the glyph's centre
            seg[i][0] = (gl->s[i][0] - 2.0f) * u, seg[i][1] = (gl->s[i][1] - 3.0f) * u;
            seg[i][2] = (gl->s[i][2] - 2.0f) * u, seg[i][3] = (gl->s[i][3] - 3.0f) * u;
        }
        glyph(seg, gl->n, gx, cy, 2.0f * u, 3.0f * u, stroke, r, g, b, a);
    }
}

bool visible() { return !g_last_pad || g_last_touch >= g_last_pad; }

}  // namespace

bool enabled() {
#ifdef __ANDROID__
    return true;
#else
    static const bool on = ps2::os::env("PS2_TOUCH") == "1";
    return on;
#endif
}

bool event(const SDL_Event &e, int w, int h) {
    if (!enabled()) return false;
    g_w = std::max(w, 1);
    g_h = std::max(h, 1);
    switch (e.type) {
    case SDL_CONTROLLERBUTTONDOWN: g_last_pad = SDL_GetTicks(); return false;
    case SDL_CONTROLLERAXISMOTION:
        if (std::abs(e.caxis.value) > 12000) g_last_pad = SDL_GetTicks();
        return false;
    case SDL_KEYDOWN:
        if (!e.key.repeat) g_last_pad = SDL_GetTicks();  // a keyboard player does not need the controls either
        return false;
    case SDL_FINGERDOWN: finger_down(e.tfinger.fingerId, e.tfinger.x * g_w, e.tfinger.y * g_h); return true;
    case SDL_FINGERMOTION: finger_move(e.tfinger.fingerId, e.tfinger.x * g_w, e.tfinger.y * g_h); return true;
    case SDL_FINGERUP: finger_up(e.tfinger.fingerId); return true;
    // PS2_TOUCH=1 on a computer: the left mouse button is a finger (window pixels: SDL's mouse coordinates are in window points,
    // the same on most displays; a scaled display only shifts where a press lands a little)
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (e.button.which == SDL_TOUCH_MOUSEID || e.button.button != SDL_BUTTON_LEFT) return false;
        if (e.type == SDL_MOUSEBUTTONDOWN) finger_down(kMouseFinger, static_cast<float>(e.button.x), static_cast<float>(e.button.y));
        else finger_up(kMouseFinger);
        return true;
    case SDL_MOUSEMOTION:
        if (e.motion.which == SDL_TOUCH_MOUSEID || !(e.motion.state & SDL_BUTTON_LMASK)) return false;
        finger_move(kMouseFinger, static_cast<float>(e.motion.x), static_cast<float>(e.motion.y));
        return true;
    default: return false;
    }
}

void pad(std::uint16_t &pressed, std::uint8_t &lx, std::uint8_t &ly) {
    if (!enabled()) return;
    const unsigned down = down_now();
    for (int i = 0; i < kCount; ++i)
        if (down & (1u << i)) pressed |= kButtons[i].bit;
    if (!g_stick_on) return;
    float dx, dy;
    stick(dx, dy);
    lx = static_cast<std::uint8_t>(std::clamp(128.0f + dx * 127.0f, 0.0f, 255.0f));
    ly = static_cast<std::uint8_t>(std::clamp(128.0f + dy * 127.0f, 0.0f, 255.0f));
    // the d-pad too, for menus that read only that: a push past half the reach
    if (dx > 0.5f) pressed |= kRight;
    if (dx < -0.5f) pressed |= kLeft;
    if (dy > 0.5f) pressed |= kDown;
    if (dy < -0.5f) pressed |= kUp;
}

bool take_options() {
    if (!g_options) return false;
    g_options = false;
    release_all();
    return true;
}

void release_all() {
    for (Finger &f : g_fingers) f.used = false;
    g_pressed = g_held = 0;
    g_stick_on = false;
}

void draw(int w, int h, bool hidden) {
    if (!enabled() || w <= 0 || h <= 0) return;
    g_w = w;
    g_h = h;
    g_shapes.clear();
    g_segments.clear();
    if (!hidden && visible()) {
        const float H = static_cast<float>(h), rim = H * 0.0035f, shadow_soft = H * 0.02f, shadow_dy = H * 0.006f;
        const unsigned now_down = down_now();
        for (int i = 0; i < kCount; ++i) {
            const Button &b = kButtons[i];
            const bool down = (now_down & (1u << i)) != 0;
            float x0, y0, x1, y1;
            place(b, x0, y0, x1, y1);
            const float k = down ? 0.93f : 1.0f, cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hw = (x1 - x0) * 0.5f * k, hh = (y1 - y0) * 0.5f * k;
            const float rad = b.round ? hw : hh;
            if (!down) shape(cx, cy + shadow_dy, hw, hh, rad, shadow_soft, 0, 0, 0, 0.30f, 0, 0, 0, 0, 0);  // the button above the picture
            if (down) shape(cx, cy, hw, hh, rad, 0, b.r * 0.85f, b.g * 0.85f, b.b * 0.85f, 0.80f, rim, 1, 1, 1, 0.85f);
            else shape(cx, cy, hw, hh, rad, 0, 0.07f, 0.08f, 0.10f, 0.42f, rim, b.r, b.g, b.b, b.round && b.anchor != kFromCentre ? 0.90f : 0.45f);
            const float lr = down ? 1.0f : b.r, lg = down ? 1.0f : b.g, lb = down ? 1.0f : b.b;
            if (b.label[0] < 0x20) {
                label(b.label, cx, cy, hh * (b.bit ? 0.95f : 1.05f), lr, lg, lb, 0.95f);
            } else {  // letters: as high as fits the button, both ways
                const float n = static_cast<float>(std::strlen(b.label)), per_h = (5.6f * (n - 1.0f) + 4.0f) / 6.0f;
                label(b.label, cx, cy, std::min(0.80f * hh, 1.55f * hw / per_h), lr, lg, lb, 0.95f);
            }
        }
        // the stick: where the thumb landed, or faintly at its usual place
        const float R = H * kStickR, knob = H * 0.052f;
        float ox = kStickX * H, oy = kStickY * H, dx = 0, dy = 0;
        if (g_stick_on) {
            ox = g_stick_ox;
            oy = g_stick_oy;
            stick(dx, dy);
            dx *= R;
            dy *= R;
        }
        const float on = g_stick_on ? 1.0f : 0.0f;
        shape(ox, oy, R, R, R, 0, 0.07f, 0.08f, 0.10f, 0.18f + 0.17f * on, rim, 1, 1, 1, 0.22f + 0.23f * on);
        shape(ox + dx, oy + dy + shadow_dy, knob, knob, knob, shadow_soft, 0, 0, 0, 0.15f + 0.20f * on, 0, 0, 0, 0, 0);
        shape(ox + dx, oy + dy, knob, knob, knob, 0, 0.90f, 0.91f, 0.94f, 0.30f + 0.60f * on, rim, 1, 1, 1, 0.35f + 0.60f * on);
    }
    // to the present pass only when something changed (the buffers are written then)
    const bool same = g_sent_any && w == g_sent_w && h == g_sent_h && g_shapes.size() == g_sent_shapes.size() && g_segments == g_sent_segments &&
                      (g_shapes.empty() || std::memcmp(g_shapes.data(), g_sent_shapes.data(), g_shapes.size() * sizeof g_shapes[0]) == 0);
    if (same) return;
    ps2::gs::gpu::set_touch_shapes(g_shapes.data(), static_cast<std::uint32_t>(g_shapes.size()), g_segments.data(),
                                   static_cast<std::uint32_t>(g_segments.size() / 4), static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h));
    g_sent_shapes = g_shapes;
    g_sent_segments = g_segments;
    g_sent_w = w;
    g_sent_h = h;
    g_sent_any = true;
}

}  // namespace touch
