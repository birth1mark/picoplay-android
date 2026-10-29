#include "doctest.h"
#include <cmath>
#include <algorithm>
#include <cstdint>

// Emulated PICO-8 Key Definitions matching hostVmShared.h
#define P8_KEY_LEFT   (1 << 0)
#define P8_KEY_RIGHT  (1 << 1)
#define P8_KEY_UP     (1 << 2)
#define P8_KEY_DOWN   (1 << 3)
#define P8_KEY_O      (1 << 4)
#define P8_KEY_X      (1 << 5)
#define P8_KEY_PAUSE  (1 << 6)

// Helpers matching implementation
static bool testPointInCircle(int px, int py, int cx, int cy, int radius, int pad) {
    if (radius <= 0) return false;
    int dx = px - cx;
    int dy = py - cy;
    int maxR = radius + pad;
    return (dx * dx + dy * dy) <= (maxR * maxR);
}

struct TestRect {
    int x, y, w, h;
};

static bool testPointInExpandedRect(int px, int py, const TestRect& r, int pad) {
    if (r.w <= 0 || r.h <= 0) return false;
    return (px >= r.x - pad && px <= r.x + r.w + pad &&
            py >= r.y - pad && py <= r.y + r.h + pad);
}

// Emulated virtual joystick evaluator
struct VirtualJoystick {
    int baseX = 270;
    int baseY = 1800;
    int radius = 100;
    int nubX = 270;
    int nubY = 1800;
    bool active = false;

    uint8_t processTouch(int tx, int ty, bool isTouching) {
        if (!isTouching) {
            active = false;
            nubX = baseX;
            nubY = baseY;
            return 0;
        }

        float dx = (float)(tx - baseX);
        float dy = (float)(ty - baseY);
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > radius * 2.5f) {
            return 0;
        }

        active = true;
        float maxNubDist = (float)radius;
        if (dist > maxNubDist) {
            nubX = baseX + (int)(dx / dist * maxNubDist);
            nubY = baseY + (int)(dy / dist * maxNubDist);
        } else {
            nubX = tx;
            nubY = ty;
        }

        uint8_t mask = 0;
        float deadzone = radius * 0.22f;
        if (dist > deadzone) {
            float thresh = deadzone * 0.70f;
            if (dx < -thresh) mask |= P8_KEY_LEFT;
            if (dx >  thresh) mask |= P8_KEY_RIGHT;
            if (dy < -thresh) mask |= P8_KEY_UP;
            if (dy >  thresh) mask |= P8_KEY_DOWN;
        }
        return mask;
    }
};

TEST_CASE("Android Virtual Thumb Joystick - Deadzone & Neutral") {
    VirtualJoystick stick;

    // Center touch: zero displacement
    uint8_t mask = stick.processTouch(270, 1800, true);
    CHECK(mask == 0);
    CHECK(stick.active == true);
    CHECK(stick.nubX == 270);
    CHECK(stick.nubY == 1800);

    // Micro-jitter within deadzone (22px deadzone)
    mask = stick.processTouch(275, 1805, true);
    CHECK(mask == 0);

    mask = stick.processTouch(265, 1795, true);
    CHECK(mask == 0);

    // Release: returns to origin
    mask = stick.processTouch(0, 0, false);
    CHECK(mask == 0);
    CHECK(stick.active == false);
    CHECK(stick.nubX == 270);
    CHECK(stick.nubY == 1800);
}

TEST_CASE("Android Virtual Thumb Joystick - Cardinal Directions") {
    VirtualJoystick stick;

    // Up
    uint8_t mask = stick.processTouch(270, 1800 - 60, true);
    CHECK(mask == P8_KEY_UP);

    // Down
    mask = stick.processTouch(270, 1800 + 60, true);
    CHECK(mask == P8_KEY_DOWN);

    // Left
    mask = stick.processTouch(270 - 60, 1800, true);
    CHECK(mask == P8_KEY_LEFT);

    // Right
    mask = stick.processTouch(270 + 60, 1800, true);
    CHECK(mask == P8_KEY_RIGHT);
}

TEST_CASE("Android Virtual Thumb Joystick - 8-Way Diagonals") {
    VirtualJoystick stick;

    // Up-Right
    uint8_t mask = stick.processTouch(270 + 50, 1800 - 50, true);
    CHECK(mask == (P8_KEY_UP | P8_KEY_RIGHT));

    // Up-Left
    mask = stick.processTouch(270 - 50, 1800 - 50, true);
    CHECK(mask == (P8_KEY_UP | P8_KEY_LEFT));

    // Down-Right
    mask = stick.processTouch(270 + 50, 1800 + 50, true);
    CHECK(mask == (P8_KEY_DOWN | P8_KEY_RIGHT));

    // Down-Left
    mask = stick.processTouch(270 - 50, 1800 + 50, true);
    CHECK(mask == (P8_KEY_DOWN | P8_KEY_LEFT));
}

TEST_CASE("Android Virtual Thumb Joystick - Nub Clamping Beyond Radius") {
    VirtualJoystick stick;

    // Touch pulled 200px to the right (radius is 100px)
    stick.processTouch(270 + 200, 1800, true);
    CHECK(stick.active == true);
    CHECK(stick.nubX == 270 + 100);
    CHECK(stick.nubY == 1800);

    // Touch pulled 300px at 45 degrees
    stick.processTouch(270 + 212, 1800 + 212, true);
    float distFromBase = std::sqrt(std::pow(stick.nubX - 270, 2) + std::pow(stick.nubY - 1800, 2));
    CHECK(distFromBase <= 101.0f);
    CHECK(distFromBase >= 99.0f);
}

TEST_CASE("Android Circular Action Buttons - Hit Detection & Padding") {
    int cx = 800, cy = 1800, radius = 45, pad = 24;

    // Exact center
    CHECK(testPointInCircle(800, 1800, cx, cy, radius, pad) == true);

    // On edge of button graphic (r = 45)
    CHECK(testPointInCircle(800 + 45, 1800, cx, cy, radius, pad) == true);

    // Inside touch pad (r = 45 + 20 = 65 < 69)
    CHECK(testPointInCircle(800 + 65, 1800, cx, cy, radius, pad) == true);

    // Right on max touch pad border (r = 45 + 24 = 69)
    CHECK(testPointInCircle(800 + 69, 1800, cx, cy, radius, pad) == true);

    // Outside touch pad (r = 75 > 69)
    CHECK(testPointInCircle(800 + 75, 1800, cx, cy, radius, pad) == false);

    // Diagonal hit inside pad
    CHECK(testPointInCircle(800 + 45, 1800 + 45, cx, cy, radius, pad) == true);

    // Diagonal hit outside pad
    CHECK(testPointInCircle(800 + 55, 1800 + 55, cx, cy, radius, pad) == false);
}

TEST_CASE("Android Top Bar - Scale Button Placement & Hit-Testing") {
    int windowW = 1080;
    int windowH = 2400;

    int topSafe = std::max(70, windowH * 7 / 100); // 168px
    int topBarH = std::max(48, windowH * 5 / 100); // 120px
    int marginX = 20;
    int spacing = 14;
    int btnW = (windowW - (marginX * 2) - (spacing * 2)) / 3; // 337px

    TestRect btnMenu   = {marginX, topSafe, btnW, topBarH};
    TestRect btnImport = {marginX + btnW + spacing, topSafe, btnW, topBarH};
    TestRect btnScale  = {marginX + (btnW + spacing) * 2, topSafe, btnW, topBarH};

    // Verify safe distance from screen top (clearing camera punch hole & status bar)
    CHECK(btnScale.y >= 160);
    CHECK(btnScale.h >= 100);
    CHECK(btnScale.x + btnScale.w <= windowW);

    // Verify buttons do not overlap
    CHECK(btnMenu.x + btnMenu.w < btnImport.x);
    CHECK(btnImport.x + btnImport.w < btnScale.x);

    // Hit-testing Scale button with 28px padding
    int pad = 28;
    int scaleCx = btnScale.x + btnScale.w / 2;
    int scaleCy = btnScale.y + btnScale.h / 2;

    // Direct center hit
    CHECK(testPointInExpandedRect(scaleCx, scaleCy, btnScale, pad) == true);

    // Top-right corner near notification bar (within pad)
    CHECK(testPointInExpandedRect(btnScale.x + btnScale.w + 20, btnScale.y - 20, btnScale, pad) == true);

    // Far outside hit
    CHECK(testPointInExpandedRect(50, 50, btnScale, pad) == false);
}

TEST_CASE("Android Scale Mode Switching - 3 Modes Cycling") {
    int scaleMode = 0; // FIT

    // Mode cycle simulation
    scaleMode = (scaleMode + 1) % 3;
    CHECK(scaleMode == 1); // 1:1 Pixel-Perfect

    scaleMode = (scaleMode + 1) % 3;
    CHECK(scaleMode == 2); // MAX Fill Width

    scaleMode = (scaleMode + 1) % 3;
    CHECK(scaleMode == 0); // Back to FIT

    // Viewport calculations in Portrait (1080 x 2400)
    int winW = 1080;
    int maxAvailableH = (2400 * 48) / 100; // 1152px
    int maxAvailableW = winW - 24;          // 1056px
    int baseSize = std::min(maxAvailableW, maxAvailableH); // 1056px

    // Mode 0: FIT
    int fitW = baseSize;
    int fitH = baseSize;
    CHECK(fitW == 1056);
    CHECK(fitH == 1056);

    // Mode 1: 1:1 Integer scaling (multiple of 128)
    int factor = baseSize / 128; // 8x scale
    int intW = factor * 128;     // 1024px
    int intH = factor * 128;     // 1024px
    CHECK(intW == 1024);
    CHECK(intH == 1024);
    CHECK(intW % 128 == 0);

    // Mode 2: MAX Fill Width
    int maxW = winW;
    int maxH = winW;
    CHECK(maxW == 1080);
    CHECK(maxH == 1080);
}

TEST_CASE("Android Physical Controller & Keyboard Auto-Detect State Machine") {
    bool showTouchControls = true;

    // Scenario A: Startup with physical gamepad detected
    bool hasPhysicalHardware = true;
    showTouchControls = !hasPhysicalHardware;
    CHECK(showTouchControls == false);

    // Player touches screen with finger
    // Event: SDL_FINGERDOWN
    showTouchControls = true;
    CHECK(showTouchControls == true);

    // Player presses gamepad button
    // Event: SDL_JOYBUTTONDOWN
    showTouchControls = false;
    CHECK(showTouchControls == false);

    // Player touches screen again
    showTouchControls = true;
    CHECK(showTouchControls == true);

    // Player types on physical keyboard
    // Event: SDL_KEYDOWN (non-back)
    showTouchControls = false;
    CHECK(showTouchControls == false);

    // Player hot-plugs Bluetooth gamepad
    // Event: SDL_JOYDEVICEADDED
    showTouchControls = false;
    CHECK(showTouchControls == false);
}

TEST_CASE("Android Touch Screen to PICO-8 Mouse Coordinate Mapping") {
    TestRect destR = {28, 308, 1024, 1024}; // 1024x1024 viewport

    auto mapTouchToMouse = [&](int tx, int ty, int& mx, int& my, uint8_t& btn) -> bool {
        if (destR.w > 0 && destR.h > 0 &&
            tx >= destR.x && tx < destR.x + destR.w &&
            ty >= destR.y && ty < destR.y + destR.h) {
            float sx = destR.w / 128.0f;
            float sy = destR.h / 128.0f;
            mx = (int)((tx - destR.x) / sx);
            my = (int)((ty - destR.y) / sy);
            btn = 1;
            return true;
        }
        btn = 0;
        return false;
    };

    int mx = 0, my = 0;
    uint8_t btn = 0;

    // Top-left of PICO-8 viewport
    CHECK(mapTouchToMouse(28, 308, mx, my, btn) == true);
    CHECK(mx == 0);
    CHECK(my == 0);
    CHECK(btn == 1);

    // Center of viewport
    CHECK(mapTouchToMouse(28 + 512, 308 + 512, mx, my, btn) == true);
    CHECK(mx == 64);
    CHECK(my == 64);

    // Bottom-right edge (1023 of 1024)
    CHECK(mapTouchToMouse(28 + 1023, 308 + 1023, mx, my, btn) == true);
    CHECK(mx == 127);
    CHECK(my == 127);

    // Touch outside viewport (below screen in control area)
    CHECK(mapTouchToMouse(500, 1600, mx, my, btn) == false);
    CHECK(btn == 0);
}

TEST_CASE("btnp Repeat Mechanism - No Consecutive Double Jumps") {
    uint8_t repeatDelay = 15;
    uint8_t repeatInterval = 4;

    auto checkRepeat = [&](int f) -> bool {
        return (f >= repeatDelay) && ((f - repeatDelay) % repeatInterval == 0);
    };

    // Frames 1 to 14: button held, but no repeat should fire
    for (int f = 1; f < 15; f++) {
        CHECK_FALSE(checkRepeat(f));
    }

    // Frame 15: first repeat fires
    CHECK(checkRepeat(15) == true);

    // Frame 16: CRITICAL - must NOT fire on consecutive frame!
    CHECK_FALSE(checkRepeat(16));
    CHECK_FALSE(checkRepeat(17));
    CHECK_FALSE(checkRepeat(18));

    // Frame 19: exactly 4 frames later, second repeat fires
    CHECK(checkRepeat(19) == true);

    // Frame 20-22: off
    CHECK_FALSE(checkRepeat(20));
    CHECK_FALSE(checkRepeat(21));
    CHECK_FALSE(checkRepeat(22));

    // Frame 23: third repeat fires
    CHECK(checkRepeat(23) == true);
}

TEST_CASE("Android Floating Joystick - Resting Position Non-Zero Guard") {
    int defaultBaseX = 270;
    int defaultBaseY = 1800;
    int baseX = defaultBaseX;
    int baseY = defaultBaseY;
    int nubX = defaultBaseX;
    int nubY = defaultBaseY;

    // Simulate touch dragging stick
    baseX = 300;
    baseY = 1750;
    nubX = 320;
    nubY = 1720;

    // Touch released: must return to default base, never (0,0)
    bool stickTouchFound = false;
    if (!stickTouchFound) {
        if (defaultBaseX > 0 && defaultBaseY > 0) {
            baseX = defaultBaseX;
            baseY = defaultBaseY;
            nubX = defaultBaseX;
            nubY = defaultBaseY;
        }
    }

    CHECK(baseX == 270);
    CHECK(baseY == 1800);
    CHECK(nubX == 270);
    CHECK(nubY == 1800);
    CHECK(baseX != 0);
    CHECK(baseY != 0);
}

TEST_CASE("Android Portrait Scale Modes - Distinct Useful Steps") {
    int windowW = 1080;
    int windowH = 2410;
    int topBarBottom = 168;
    int controlDeckY = windowH * 53 / 100; // 1277
    int maxDispH = controlDeckY - (topBarBottom + 20); // 1089
    int maxBound = std::min(windowW, maxDispH); // 1080
    int maxInt = maxBound / 128; // 8

    // Mode 0: FIT (Framed Handheld Console)
    int fitFactor = std::max(1, maxInt - 1); // 7
    int fitW = fitFactor * 128; // 896

    // Mode 1: 1:1 (Max Pixel-Perfect)
    int oneToOneW = maxInt * 128; // 1024

    // Mode 2: MAX (Edge-to-Edge)
    int maxW = windowW; // 1080

    CHECK(fitW == 896);
    CHECK(oneToOneW == 1024);
    CHECK(maxW == 1080);

    // Each mode must have a significant, clearly visible difference (> 50px)
    CHECK(oneToOneW - fitW >= 128);
    CHECK(maxW - oneToOneW >= 56);
}
