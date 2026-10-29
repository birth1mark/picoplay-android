#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <string>
#include "../libs/lodepng/lodepng.h"

// Emulated PICO-8 / PicoPlay Key Definitions
#define P8_KEY_LEFT   (1 << 0)
#define P8_KEY_RIGHT  (1 << 1)
#define P8_KEY_UP     (1 << 2)
#define P8_KEY_DOWN   (1 << 3)
#define P8_KEY_O      (1 << 4)
#define P8_KEY_X      (1 << 5)
#define P8_KEY_PAUSE  (1 << 6)

static const int PicoScreenWidth = 128;
static const int PicoScreenHeight = 128;

// Layout variables
static int _windowWidth = 1080;
static int _windowHeight = 2400;
static int g_scaleMode = 0; // 0 = Fit Handheld, 1 = Pixel Perfect, 2 = MAX Fill
static SDL_Rect DestR = {0, 0, 0, 0};

static int g_stickBaseX = 0;
static int g_stickBaseY = 0;
static int g_stickNubX = 0;
static int g_stickNubY = 0;
static int g_stickRadius = 80;
static int g_stickNubRadius = 36;
static bool g_stickActive = false;

static int g_btnOCenterX = 0, g_btnOCenterY = 0, g_btnORadius = 45;
static int g_btnXCenterX = 0, g_btnXCenterY = 0, g_btnXRadius = 45;

static SDL_Rect g_btnMenuRect = {0, 0, 0, 0};
static SDL_Rect g_btnImportRect = {0, 0, 0, 0};
static SDL_Rect g_btnScaleRect = {0, 0, 0, 0};

static uint8_t g_androidTouchHeldMask = 0;

// Rendering primitives
static void renderFilledCircle(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    if (radius <= 0) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    std::vector<SDL_Rect> rects;
    rects.reserve(radius * 2 + 1);
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = (int)sqrt(radius * radius - dy * dy);
        rects.push_back({cx - dx, cy + dy, dx * 2 + 1, 1});
    }
    SDL_RenderFillRects(r, rects.data(), (int)rects.size());
}

static void renderCircleRing(SDL_Renderer* r, int cx, int cy, int radius, int thickness, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    if (radius <= 0 || thickness <= 0) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    std::vector<SDL_Point> pts;
    pts.reserve(radius * 8 * thickness);
    for (int t = 0; t < thickness; t++) {
        int rad = radius - t;
        if (rad <= 0) break;
        int x = rad, y = 0;
        int p = 1 - rad;
        while (x >= y) {
            pts.push_back({cx + x, cy + y});
            pts.push_back({cx - x, cy + y});
            pts.push_back({cx + x, cy - y});
            pts.push_back({cx - x, cy - y});
            pts.push_back({cx + y, cy + x});
            pts.push_back({cx - y, cy + x});
            pts.push_back({cx + y, cy - x});
            pts.push_back({cx - y, cy - x});
            y++;
            if (p <= 0) {
                p += 2 * y + 1;
            } else {
                x--;
                p += 2 * (y - x) + 1;
            }
        }
    }
    if (!pts.empty()) {
        SDL_RenderDrawPoints(r, pts.data(), (int)pts.size());
    }
}

static void renderRoundedRect(SDL_Renderer* r, const SDL_Rect& rect, int radius, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    if (rect.w <= 0 || rect.h <= 0) return;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    int rad = std::min(radius, std::min(rect.w / 2, rect.h / 2));
    if (rad <= 0) {
        SDL_RenderFillRect(r, &rect);
        return;
    }
    SDL_Rect mid = {rect.x, rect.y + rad, rect.w, rect.h - rad * 2};
    SDL_RenderFillRect(r, &mid);
    SDL_Rect top = {rect.x + rad, rect.y, rect.w - rad * 2, rad};
    SDL_Rect bot = {rect.x + rad, rect.y + rect.h - rad, rect.w - rad * 2, rad};
    SDL_RenderFillRect(r, &top);
    SDL_RenderFillRect(r, &bot);
    renderFilledCircle(r, rect.x + rad, rect.y + rad, rad, R, G, B, A);
    renderFilledCircle(r, rect.x + rect.w - rad - 1, rect.y + rad, rad, R, G, B, A);
    renderFilledCircle(r, rect.x + rad, rect.y + rect.h - rad - 1, rad, R, G, B, A);
    renderFilledCircle(r, rect.x + rect.w - rad - 1, rect.y + rect.h - rad - 1, rad, R, G, B, A);
}

// Draw a filled triangle for D-pad directional arrows
static void renderFilledTriangle(SDL_Renderer* r, int x0, int y0, int x1, int y1, int x2, int y2, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    if (y0 > y1) { std::swap(x0, x1); std::swap(y0, y1); }
    if (y0 > y2) { std::swap(x0, x2); std::swap(y0, y2); }
    if (y1 > y2) { std::swap(x1, x2); std::swap(y1, y2); }

    int total_h = y2 - y0;
    if (total_h == 0) return;

    for (int y = y0; y <= y2; y++) {
        bool second_half = y > y1 || y1 == y0;
        int segment_h = second_half ? (y2 - y1) : (y1 - y0);
        if (segment_h == 0) continue;
        float alpha = (float)(y - y0) / total_h;
        float beta  = (float)(y - (second_half ? y1 : y0)) / segment_h;
        int ax = x0 + (int)((x2 - x0) * alpha);
        int bx = second_half ? (x1 + (int)((x2 - x1) * beta)) : (x0 + (int)((x1 - x0) * beta));
        if (ax > bx) std::swap(ax, bx);
        SDL_RenderDrawLine(r, ax, y, bx, y);
    }
}

// 3x5 Pixel Font
static uint16_t getGlyph3x5(char c) {
    switch (c) {
        case 'A': return 0b111101111101101;
        case 'B': return 0b110101110101110;
        case 'C': return 0b111100100100111;
        case 'D': return 0b110101101101110;
        case 'E': return 0b111100111100111;
        case 'F': return 0b111100110100100;
        case 'G': return 0b011100101101011;
        case 'H': return 0b101101111101101;
        case 'I': return 0b111010010010111;
        case 'K': return 0b101101110101101;
        case 'L': return 0b100100100100111;
        case 'M': return 0b101111101101101;
        case 'N': return 0b101110101011101;
        case 'O': return 0b111101101101111;
        case 'P': return 0b111101111100100;
        case 'R': return 0b110101110101101;
        case 'S': return 0b011100010001110;
        case 'T': return 0b111010010010010;
        case 'U': return 0b101101101101111;
        case 'V': return 0b101101101101010;
        case 'X': return 0b101101010101101;
        case 'Y': return 0b101101111010010;
        case '0': return 0b111101101101111;
        case '1': return 0b010110010010111;
        case '2': return 0b111001111100111;
        case '3': return 0b111001111001111;
        case '8': return 0b111101111101111;
        case ':': return 0b000010000010000;
        case '+': return 0b000010111010000;
        case '-': return 0b000000111000000;
        case ' ': return 0b000000000000000;
        default:  return 0;
    }
}

static void drawPixelText(SDL_Renderer* r, const char* text, int cx, int cy, int scale, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    int len = (int)strlen(text);
    if (len <= 0) return;
    int totalW = len * 4 * scale - scale;
    int startX = cx - totalW / 2;
    int startY = cy - (5 * scale) / 2;
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, R, G, B, A);
    std::vector<SDL_Rect> pxs;
    pxs.reserve(len * 15);
    for (int i = 0; i < len; i++) {
        uint16_t g = getGlyph3x5(text[i]);
        int gx = startX + i * 4 * scale;
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 3; col++) {
                if ((g >> (14 - (row * 3 + col))) & 1) {
                    pxs.push_back({gx + col * scale, startY + row * scale, scale, scale});
                }
            }
        }
    }
    if (!pxs.empty()) {
        SDL_RenderFillRects(r, pxs.data(), (int)pxs.size());
    }
}

// Dedicated High-Fidelity Action Button Glyphs
static void drawBoldButtonOGlyph(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    int glyphR = radius * 38 / 100;
    int thick = std::max(3, radius * 14 / 100);
    // Bevel shadow
    renderCircleRing(r, cx, cy + 2, glyphR, thick, 0, 0, 0, 160);
    // Pure glyph ring
    renderCircleRing(r, cx, cy, glyphR, thick, R, G, B, A);
}

static void drawBoldButtonXGlyph(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    int armLen = radius * 35 / 100;
    int thick = std::max(3, radius * 13 / 100);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // Shadow
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    for (int t = -thick / 2; t <= thick / 2; t++) {
        SDL_RenderDrawLine(r, cx - armLen + t, cy - armLen + 2, cx + armLen + t, cy + armLen + 2);
        SDL_RenderDrawLine(r, cx - armLen + t, cy + armLen + 2, cx + armLen + t, cy - armLen + 2);
    }
    // Main white stroke
    SDL_SetRenderDrawColor(r, R, G, B, A);
    for (int t = -thick / 2; t <= thick / 2; t++) {
        SDL_RenderDrawLine(r, cx - armLen + t, cy - armLen, cx + armLen + t, cy + armLen);
        SDL_RenderDrawLine(r, cx - armLen + t, cy + armLen, cx + armLen + t, cy - armLen);
    }
}

// Layout update
static void updateLayout() {
    bool portrait = (_windowHeight >= _windowWidth);
    if (portrait) {
        int topSafe = std::max(60, _windowHeight * 5 / 100);
        int topBarH = std::max(48, _windowHeight * 4 / 100);
        int topBarBottom = topSafe + topBarH;

        int marginX = 24;
        int spacing = 16;
        int btnW = (_windowWidth - (marginX * 2) - (spacing * 2)) / 3;

        g_btnMenuRect   = {marginX, topSafe, btnW, topBarH};
        g_btnImportRect = {marginX + btnW + spacing, topSafe, btnW, topBarH};
        g_btnScaleRect  = {marginX + (btnW + spacing) * 2, topSafe, btnW, topBarH};

        int controlDeckY = _windowHeight * 55 / 100;
        int controlDeckH = _windowHeight - controlDeckY;

        g_stickRadius = std::min(_windowWidth * 19 / 100, controlDeckH * 32 / 100);
        if (g_stickRadius < 65) g_stickRadius = 65;
        g_stickNubRadius = g_stickRadius * 46 / 100;
        g_stickBaseX = _windowWidth * 25 / 100;
        g_stickBaseY = controlDeckY + controlDeckH * 48 / 100;

        if (!g_stickActive) {
            g_stickNubX = g_stickBaseX;
            g_stickNubY = g_stickBaseY;
        }

        g_btnORadius = g_stickRadius * 52 / 100;
        g_btnXRadius = g_btnORadius;

        int rightHubX = _windowWidth * 75 / 100;
        int rightHubY = g_stickBaseY;
        g_btnOCenterX = rightHubX - g_btnORadius - 8;
        g_btnOCenterY = rightHubY + g_btnORadius - 4;

        g_btnXCenterX = rightHubX + g_btnXRadius + 8;
        g_btnXCenterY = rightHubY - g_btnXRadius + 8;

        int displayAreaTop = topBarBottom + 16;
        int displayAreaH = controlDeckY - displayAreaTop - 16;
        int maxBound = std::min(_windowWidth, displayAreaH);
        int maxIntegerFactor = maxBound / PicoScreenWidth;
        if (maxIntegerFactor < 1) maxIntegerFactor = 1;

        if (g_scaleMode == 1) {
            int sw = maxIntegerFactor * PicoScreenWidth;
            int sh = maxIntegerFactor * PicoScreenHeight;
            DestR = {(_windowWidth - sw) / 2, displayAreaTop + (displayAreaH - sh) / 2, sw, sh};
        } else if (g_scaleMode == 2) {
            DestR = {0, displayAreaTop + (displayAreaH - _windowWidth) / 2, _windowWidth, _windowWidth};
            if (DestR.y < displayAreaTop) DestR.y = displayAreaTop;
        } else {
            int fitFactor = std::max(1, maxIntegerFactor - 1);
            int sw = fitFactor * PicoScreenWidth;
            int sh = fitFactor * PicoScreenHeight;
            DestR = {(_windowWidth - sw) / 2, displayAreaTop + (displayAreaH - sh) / 2, sw, sh};
        }
    } else {
        // Landscape layout
        int maxScreen = _windowHeight - 24;
        int maxInt = maxScreen / PicoScreenWidth;
        if (maxInt < 1) maxInt = 1;

        if (g_scaleMode == 1) {
            DestR = {(_windowWidth - maxInt * PicoScreenWidth) / 2, (_windowHeight - maxInt * PicoScreenHeight) / 2, maxInt * PicoScreenWidth, maxInt * PicoScreenHeight};
        } else if (g_scaleMode == 2) {
            DestR = {(_windowWidth - _windowHeight) / 2, 0, _windowHeight, _windowHeight};
        } else {
            int fitFactor = std::max(1, maxInt - 1);
            DestR = {(_windowWidth - fitFactor * PicoScreenWidth) / 2, (_windowHeight - fitFactor * PicoScreenHeight) / 2, fitFactor * PicoScreenWidth, fitFactor * PicoScreenHeight};
        }

        int sideW = std::max(160, DestR.x - 16);
        int topBtnH = std::max(40, _windowHeight / 12);
        int topBtnW = sideW * 44 / 100;
        int topSafeLand = std::max(28, _windowHeight * 4 / 100);

        g_btnMenuRect   = {20, topSafeLand, topBtnW, topBtnH};
        g_btnImportRect = {20 + topBtnW + 12, topSafeLand, topBtnW, topBtnH};
        g_btnScaleRect  = {_windowWidth - topBtnW - 20, topSafeLand, topBtnW, topBtnH};

        g_stickRadius = std::min(sideW * 38 / 100, _windowHeight * 25 / 100);
        g_stickNubRadius = g_stickRadius * 46 / 100;
        g_stickBaseX = sideW / 2 + 10;
        g_stickBaseY = _windowHeight * 64 / 100;

        if (!g_stickActive) {
            g_stickNubX = g_stickBaseX;
            g_stickNubY = g_stickBaseY;
        }

        g_btnORadius = g_stickRadius * 52 / 100;
        g_btnXRadius = g_btnORadius;
        int rightBaseX = _windowWidth - sideW / 2 - 10;
        g_btnOCenterX = rightBaseX - g_btnORadius;
        g_btnOCenterY = _windowHeight * 68 / 100;
        g_btnXCenterX = rightBaseX + g_btnXRadius;
        g_btnXCenterY = _windowHeight * 54 / 100;
    }
}

// Draw the Analogue Pocket Style Obsidian Matte Console Chassis
static void drawConsoleChassis(SDL_Renderer* r) {
    bool portrait = (_windowHeight >= _windowWidth);
    int controlDeckY = portrait ? (_windowHeight * 55 / 100) : 0;

    // 1. CHASSIS OUTER BEVEL & CNC CHAMFER
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    // Subtle top/left edge CNC highlight
    SDL_SetRenderDrawColor(r, 48, 56, 72, 160);
    SDL_RenderDrawLine(r, 0, 0, _windowWidth - 1, 0);
    SDL_RenderDrawLine(r, 0, 1, _windowWidth - 1, 1);
    SDL_RenderDrawLine(r, 0, 0, 0, _windowHeight - 1);
    SDL_RenderDrawLine(r, 1, 0, 1, _windowHeight - 1);

    // Subtle bottom/right edge CNC shadow
    SDL_SetRenderDrawColor(r, 6, 8, 11, 240);
    SDL_RenderDrawLine(r, 0, _windowHeight - 1, _windowWidth - 1, _windowHeight - 1);
    SDL_RenderDrawLine(r, 0, _windowHeight - 2, _windowWidth - 1, _windowHeight - 2);
    SDL_RenderDrawLine(r, _windowWidth - 1, 0, _windowWidth - 1, _windowHeight - 1);
    SDL_RenderDrawLine(r, _windowWidth - 2, 0, _windowWidth - 2, _windowHeight - 1);

    // 2. HARDWARE PARTING LINE / SEAM
    if (portrait) {
        int seamY = controlDeckY - 14;
        // Dark groove
        SDL_SetRenderDrawColor(r, 7, 9, 13, 255);
        SDL_RenderDrawLine(r, 12, seamY, _windowWidth - 13, seamY);
        // Bevel highlight below groove
        SDL_SetRenderDrawColor(r, 34, 40, 52, 170);
        SDL_RenderDrawLine(r, 12, seamY + 1, _windowWidth - 13, seamY + 1);
    } else {
        int leftSeamX = DestR.x - 14;
        int rightSeamX = DestR.x + DestR.w + 14;
        SDL_SetRenderDrawColor(r, 7, 9, 13, 255);
        SDL_RenderDrawLine(r, leftSeamX, 10, leftSeamX, _windowHeight - 11);
        SDL_RenderDrawLine(r, rightSeamX, 10, rightSeamX, _windowHeight - 11);
        SDL_SetRenderDrawColor(r, 34, 40, 52, 170);
        SDL_RenderDrawLine(r, leftSeamX + 1, 10, leftSeamX + 1, _windowHeight - 11);
        SDL_RenderDrawLine(r, rightSeamX + 1, 10, rightSeamX + 1, _windowHeight - 11);
    }

    // 3. DISPLAY FRAME & LAMINATED GLASS BEZEL
    if (DestR.x >= 6) {
        // Outer chamfered bezel
        SDL_Rect outerBezel = {DestR.x - 12, DestR.y - 12, DestR.w + 24, DestR.h + 24};
        // Top/left highlight
        SDL_SetRenderDrawColor(r, 38, 46, 60, 255);
        SDL_RenderDrawLine(r, outerBezel.x, outerBezel.y, outerBezel.x + outerBezel.w - 1, outerBezel.y);
        SDL_RenderDrawLine(r, outerBezel.x, outerBezel.y + 1, outerBezel.x + outerBezel.w - 2, outerBezel.y + 1);
        SDL_RenderDrawLine(r, outerBezel.x, outerBezel.y, outerBezel.x, outerBezel.y + outerBezel.h - 1);
        SDL_RenderDrawLine(r, outerBezel.x + 1, outerBezel.y, outerBezel.x + 1, outerBezel.y + outerBezel.h - 2);

        // Bottom/right shadow
        SDL_SetRenderDrawColor(r, 6, 8, 11, 255);
        SDL_RenderDrawLine(r, outerBezel.x, outerBezel.y + outerBezel.h - 1, outerBezel.x + outerBezel.w - 1, outerBezel.y + outerBezel.h - 1);
        SDL_RenderDrawLine(r, outerBezel.x + 1, outerBezel.y + outerBezel.h - 2, outerBezel.x + outerBezel.w - 1, outerBezel.y + outerBezel.h - 2);
        SDL_RenderDrawLine(r, outerBezel.x + outerBezel.w - 1, outerBezel.y, outerBezel.x + outerBezel.w - 1, outerBezel.y + outerBezel.h - 1);
        SDL_RenderDrawLine(r, outerBezel.x + outerBezel.w - 2, outerBezel.y + 1, outerBezel.x + outerBezel.w - 2, outerBezel.y + outerBezel.h - 1);

        // Piano black inner glass frame
        for (int i = 1; i <= 8; i++) {
            SDL_Rect glass = {DestR.x - i, DestR.y - i, DestR.w + i * 2, DestR.h + i * 2};
            SDL_SetRenderDrawColor(r, 7, 9, 12, 255);
            SDL_RenderDrawRect(r, &glass);
        }

        // Laminated glass top glare reflection line
        SDL_SetRenderDrawColor(r, 255, 255, 255, 48);
        SDL_RenderDrawLine(r, DestR.x - 4, DestR.y - 4, DestR.x + DestR.w + 3, DestR.y - 4);
    } else {
        // MAX Edge-to-edge mode: Top and Bottom precision integration seams
        SDL_SetRenderDrawColor(r, 7, 9, 12, 255);
        SDL_RenderDrawLine(r, 0, DestR.y - 1, _windowWidth - 1, DestR.y - 1);
        SDL_RenderDrawLine(r, 0, DestR.y + DestR.h, _windowWidth - 1, DestR.y + DestR.h);
        SDL_SetRenderDrawColor(r, 34, 40, 52, 160);
        SDL_RenderDrawLine(r, 0, DestR.y - 2, _windowWidth - 1, DestR.y - 2);
        SDL_RenderDrawLine(r, 0, DestR.y + DestR.h + 1, _windowWidth - 1, DestR.y + DestR.h + 1);
    }

    // 4. HARDWARE OPTICAL POWER / BATTERY LED
    int ledX = portrait ? (g_btnMenuRect.x + 8) : 24;
    int ledY = portrait ? (g_btnMenuRect.y - 22) : (g_btnMenuRect.y - 14);
    if (ledY < 14) ledY = 16;
    // Outer dark housing well
    renderFilledCircle(r, ledX, ledY, 6, 8, 10, 14, 255);
    renderCircleRing(r, ledX, ledY, 6, 1, 30, 36, 48, 180);
    // Soft outer diffuse halo
    renderFilledCircle(r, ledX, ledY, 8, 0, 228, 54, 40);
    // Diffuser emerald lens
    renderFilledCircle(r, ledX, ledY, 4, 0, 228, 54, 255);
    // Micro phosphor hot center
    renderFilledCircle(r, ledX - 1, ledY - 1, 2, 210, 255, 230, 255);

    // 5. DEBOSSED HARDWARE LOGO: "P  I  C  O  P  L  A  Y"
    int logoScale = std::max(2, _windowWidth / 360);
    int logoX = _windowWidth / 2;
    int logoY = portrait ? (DestR.y + DestR.h + (controlDeckY - 14 - (DestR.y + DestR.h)) / 2) : (DestR.y >= 20 ? (DestR.y / 2) : (_windowHeight - 20));
    if ((portrait && logoY > DestR.y + DestR.h + 8) || (!portrait && logoY > 10)) {
        const char* brandText = "P I C O P L A Y";
        // Deboss shadow (1px down)
        drawPixelText(r, brandText, logoX, logoY + 2, logoScale, 5, 7, 9, 240);
        // Titanium matte debossed face
        drawPixelText(r, brandText, logoX, logoY, logoScale, 78, 88, 105, 210);
        // Micro highlight
        drawPixelText(r, brandText, logoX, logoY - 1, logoScale, 110, 122, 142, 70);
    }

    // 6. PRECISION DRILLED SPEAKER PINHOLES
    int speakerBaseX = portrait ? (_windowWidth * 80 / 100) : (_windowWidth - 80);
    int speakerBaseY = portrait ? (_windowHeight - 48) : (_windowHeight - 40);
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 5; col++) {
            int hx = speakerBaseX + col * 13;
            int hy = speakerBaseY + row * 13;
            // Dark cavity
            renderFilledCircle(r, hx, hy, 3, 5, 7, 10, 255);
            // Lower drilled rim highlight
            renderCircleRing(r, hx, hy + 1, 3, 1, 44, 52, 68, 170);
        }
    }

    // 7. TOP RECESSED PILLS: [MENU], [+CART], [SCALE]
    int fontScale = std::max(2, _windowWidth / 220);
    auto drawRecessedPill = [&](const SDL_Rect& rect, const char* label, Uint8 bgR, Uint8 bgG, Uint8 bgB) {
        // Outer cutout well (2px larger)
        SDL_Rect well = {rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4};
        renderRoundedRect(r, well, 10, 7, 9, 12, 255);
        // Well lower rim highlight
        SDL_SetRenderDrawColor(r, 34, 40, 52, 180);
        SDL_RenderDrawLine(r, well.x + 8, well.y + well.h - 1, well.x + well.w - 8, well.y + well.h - 1);

        // Pill button cap
        renderRoundedRect(r, rect, 8, bgR, bgG, bgB, 240);

        // Top highlight bevel line
        SDL_SetRenderDrawColor(r, 255, 255, 255, 60);
        SDL_RenderDrawLine(r, rect.x + 8, rect.y + 1, rect.x + rect.w - 8, rect.y + 1);

        // Bottom shadow bevel line
        SDL_SetRenderDrawColor(r, 0, 0, 0, 90);
        SDL_RenderDrawLine(r, rect.x + 8, rect.y + rect.h - 2, rect.x + rect.w - 8, rect.y + rect.h - 2);

        // Laser-etched text
        drawPixelText(r, label, rect.x + rect.w / 2, rect.y + rect.h / 2 + 1, fontScale, 0, 0, 0, 180);
        drawPixelText(r, label, rect.x + rect.w / 2, rect.y + rect.h / 2, fontScale, 240, 245, 255, 240);
    };

    drawRecessedPill(g_btnMenuRect,   "MENU",  24, 28, 38);
    drawRecessedPill(g_btnImportRect, "+CART", 12, 72, 48);
    const char* scaleLabel = (g_scaleMode == 1) ? "1:1" : ((g_scaleMode == 2) ? "MAX" : "FIT");
    drawRecessedPill(g_btnScaleRect,  scaleLabel, 34, 40, 52);

    // 8. MACHINED D-PAD / THUMBSTICK RECESSED DISH
    int wellR = g_stickRadius + 14;
    // Outer well cavity
    renderFilledCircle(r, g_stickBaseX, g_stickBaseY, wellR, 8, 10, 13, 255);
    // Well bevel rim: top-left shadow, bottom-right highlight
    renderCircleRing(r, g_stickBaseX, g_stickBaseY, wellR, 2, 34, 40, 52, 190);

    // Concave dish concentric steps
    renderFilledCircle(r, g_stickBaseX, g_stickBaseY, g_stickRadius + 4, 13, 16, 21, 255);
    renderCircleRing(r, g_stickBaseX, g_stickBaseY, g_stickRadius + 4, 1, 20, 24, 32, 180);
    renderFilledCircle(r, g_stickBaseX, g_stickBaseY, g_stickRadius - 8, 10, 13, 17, 255);

    // Cardinal Directional Engraved Arrows (▲, ▼, ◀, ▶)
    int markerDist = g_stickRadius - 6;
    int arrowW = std::max(6, g_stickRadius * 12 / 100);
    int arrowH = std::max(5, g_stickRadius * 10 / 100);

    // UP Arrow
    bool upHeld = (g_androidTouchHeldMask & P8_KEY_UP) != 0;
    if (upHeld) {
        renderFilledCircle(r, g_stickBaseX, g_stickBaseY - markerDist, 10, 41, 173, 255, 90);
        renderFilledTriangle(r, g_stickBaseX, g_stickBaseY - markerDist - arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY - markerDist + arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY - markerDist + arrowH,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(r, g_stickBaseX, g_stickBaseY - markerDist - arrowH + 1,
                                g_stickBaseX - arrowW, g_stickBaseY - markerDist + arrowH + 1,
                                g_stickBaseX + arrowW, g_stickBaseY - markerDist + arrowH + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(r, g_stickBaseX, g_stickBaseY - markerDist - arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY - markerDist + arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY - markerDist + arrowH,
                                60, 70, 86, 200);
    }

    // DOWN Arrow
    bool downHeld = (g_androidTouchHeldMask & P8_KEY_DOWN) != 0;
    if (downHeld) {
        renderFilledCircle(r, g_stickBaseX, g_stickBaseY + markerDist, 10, 41, 173, 255, 90);
        renderFilledTriangle(r, g_stickBaseX, g_stickBaseY + markerDist + arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY + markerDist - arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY + markerDist - arrowH,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(r, g_stickBaseX, g_stickBaseY + markerDist + arrowH + 1,
                                g_stickBaseX - arrowW, g_stickBaseY + markerDist - arrowH + 1,
                                g_stickBaseX + arrowW, g_stickBaseY + markerDist - arrowH + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(r, g_stickBaseX, g_stickBaseY + markerDist + arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY + markerDist - arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY + markerDist - arrowH,
                                60, 70, 86, 200);
    }

    // LEFT Arrow
    bool leftHeld = (g_androidTouchHeldMask & P8_KEY_LEFT) != 0;
    if (leftHeld) {
        renderFilledCircle(r, g_stickBaseX - markerDist, g_stickBaseY, 10, 41, 173, 255, 90);
        renderFilledTriangle(r, g_stickBaseX - markerDist - arrowH, g_stickBaseY,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY + arrowW,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(r, g_stickBaseX - markerDist - arrowH + 1, g_stickBaseY + 1,
                                g_stickBaseX - markerDist + arrowH + 1, g_stickBaseY - arrowW + 1,
                                g_stickBaseX - markerDist + arrowH + 1, g_stickBaseY + arrowW + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(r, g_stickBaseX - markerDist - arrowH, g_stickBaseY,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY + arrowW,
                                60, 70, 86, 200);
    }

    // RIGHT Arrow
    bool rightHeld = (g_androidTouchHeldMask & P8_KEY_RIGHT) != 0;
    if (rightHeld) {
        renderFilledCircle(r, g_stickBaseX + markerDist, g_stickBaseY, 10, 41, 173, 255, 90);
        renderFilledTriangle(r, g_stickBaseX + markerDist + arrowH, g_stickBaseY,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY + arrowW,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(r, g_stickBaseX + markerDist + arrowH + 1, g_stickBaseY + 1,
                                g_stickBaseX + markerDist - arrowH + 1, g_stickBaseY - arrowW + 1,
                                g_stickBaseX + markerDist - arrowH + 1, g_stickBaseY + arrowW + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(r, g_stickBaseX + markerDist + arrowH, g_stickBaseY,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY + arrowW,
                                60, 70, 86, 200);
    }

    // Tactile Machined Thumb Nub
    Uint8 nubFillR = g_stickActive ? 32 : 24;
    Uint8 nubFillG = g_stickActive ? 38 : 28;
    Uint8 nubFillB = g_stickActive ? 52 : 38;

    // Nub cast shadow
    if (!g_stickActive) {
        renderFilledCircle(r, g_stickNubX + 2, g_stickNubY + 5, g_stickNubRadius, 4, 5, 7, 190);
    } else {
        renderFilledCircle(r, g_stickNubX, g_stickNubY, g_stickNubRadius + 6, 41, 173, 255, 60);
    }
    // Nub cylindrical body
    renderFilledCircle(r, g_stickNubX, g_stickNubY, g_stickNubRadius, nubFillR, nubFillG, nubFillB, 255);
    // Knurled grip perimeter ring
    renderCircleRing(r, g_stickNubX, g_stickNubY, g_stickNubRadius, 3, 58, 68, 86, 220);

    // Radial knurling ticks around the perimeter (8 notch ticks)
    for (int a = 0; a < 8; a++) {
        float angle = a * (3.14159265f / 4.0f);
        int tx0 = g_stickNubX + (int)(cosf(angle) * (g_stickNubRadius - 3));
        int ty0 = g_stickNubY + (int)(sinf(angle) * (g_stickNubRadius - 3));
        int tx1 = g_stickNubX + (int)(cosf(angle) * (g_stickNubRadius + 1));
        int ty1 = g_stickNubY + (int)(sinf(angle) * (g_stickNubRadius + 1));
        SDL_SetRenderDrawColor(r, 30, 36, 46, 255);
        SDL_RenderDrawLine(r, tx0, ty0, tx1, ty1);
    }

    // Top-left convex highlight arc
    renderCircleRing(r, g_stickNubX - 1, g_stickNubY - 1, g_stickNubRadius - 2, 1, 255, 255, 255, g_stickActive ? 180 : 90);
    // Concave center dish
    renderFilledCircle(r, g_stickNubX, g_stickNubY, g_stickNubRadius * 48 / 100, 14, 16, 22, 255);
    renderCircleRing(r, g_stickNubX, g_stickNubY, g_stickNubRadius * 48 / 100, 1, 8, 10, 14, 255);
    // Center micro-dimple
    renderFilledCircle(r, g_stickNubX, g_stickNubY, 3, 44, 52, 68, 240);

    // 9. 3D MECHANICAL ACTION BUTTONS ([O] and [X])
    auto drawMechanicalButton = [&](int cx, int cy, int radius, bool pressed, bool isO, Uint8 baseR, Uint8 baseG, Uint8 baseB) {
        int wellRadius = radius + 9;
        // 1. Recessed button well drilled into chassis
        renderFilledCircle(r, cx, cy, wellRadius, 7, 9, 12, 255);
        // Well inner top shadow
        renderCircleRing(r, cx, cy, wellRadius, 2, 4, 5, 7, 240);
        // Well lower rim highlight
        renderCircleRing(r, cx, cy + 1, wellRadius, 1, 34, 40, 52, 190);

        // 2. Mechanical travel: button shifts down by +4px when pressed
        int capY = pressed ? (cy + 4) : cy;
        int capX = cx;

        // Cast shadow when unpressed
        if (!pressed) {
            renderFilledCircle(r, capX, capY + 5, radius, 3, 4, 6, 210);
        } else {
            // Active glow halo inside the well
            renderFilledCircle(r, capX, capY, radius + 5, baseR, baseG, baseB, 75);
        }

        // Button cylindrical body
        Uint8 curR = pressed ? std::min(255, baseR + 35) : baseR;
        Uint8 curG = pressed ? std::min(255, baseG + 35) : baseG;
        Uint8 curB = pressed ? std::min(255, baseB + 35) : baseB;
        renderFilledCircle(r, capX, capY, radius, curR, curG, curB, 255);

        // Outer beveled edge ring
        Uint8 rimR = pressed ? 255 : (Uint8)(baseR * 0.72f);
        Uint8 rimG = pressed ? 255 : (Uint8)(baseG * 0.72f);
        Uint8 rimB = pressed ? 255 : (Uint8)(baseG * 0.72f);
        renderCircleRing(r, capX, capY, radius, 2, rimR, rimG, rimB, 230);

        // Top-left convex dome specular highlight
        if (!pressed) {
            renderCircleRing(r, capX - 1, capY - 1, radius - 2, 2, 255, 255, 255, 95);
        }

        // Crisp bold button glyph
        if (isO) {
            drawBoldButtonOGlyph(r, capX, capY, radius, 255, 255, 255, 255);
        } else {
            drawBoldButtonXGlyph(r, capX, capY, radius, 255, 255, 255, 255);
        }
    };

    drawMechanicalButton(g_btnOCenterX, g_btnOCenterY, g_btnORadius, (g_androidTouchHeldMask & P8_KEY_O) != 0, true, 255, 0, 77);
    drawMechanicalButton(g_btnXCenterX, g_btnXCenterY, g_btnXRadius, (g_androidTouchHeldMask & P8_KEY_X) != 0, false, 41, 173, 255);
}

// Draw a mock PICO-8 screen inside DestR
static void drawMockPicoScreen(SDL_Renderer* r) {
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 255);
    SDL_RenderFillRect(r, &DestR);

    // Draw retro grid pattern
    int step = DestR.w / 16;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if ((x + y) % 2 == 0) {
                SDL_Rect tile = {DestR.x + x * step, DestR.y + y * step, step, step};
                SDL_SetRenderDrawColor(r, 29, 43, 83, 255);
                SDL_RenderFillRect(r, &tile);
            }
        }
    }

    // PICO-8 Boot Text
    int pTextScale = std::max(2, DestR.w / 280);
    drawPixelText(r, "PICOPLAY 1.0.1", DestR.x + DestR.w / 2, DestR.y + DestR.h / 3, pTextScale, 255, 236, 39, 255);
    drawPixelText(r, "OBSIDIAN EDITION", DestR.x + DestR.w / 2, DestR.y + DestR.h / 3 + 24 * pTextScale / 2, pTextScale, 41, 173, 255, 255);
    drawPixelText(r, "READY.", DestR.x + DestR.w / 2, DestR.y + DestR.h * 2 / 3, pTextScale, 0, 228, 54, 255);
}

// Save renderer surface to PNG using lodepng
static bool saveRendererToPng(SDL_Renderer* r, int w, int h, const std::string& path) {
    std::vector<uint8_t> pixels(w * h * 4);
    if (SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_RGBA32, pixels.data(), w * 4) != 0) {
        std::cerr << "Failed to read pixels: " << SDL_GetError() << std::endl;
        return false;
    }
    unsigned error = lodepng::encode(path, pixels, w, h);
    if (error) {
        std::cerr << "PNG encode error " << error << ": " << lodepng_error_text(error) << std::endl;
        return false;
    }
    std::cout << "Successfully saved: " << path << std::endl;
    return true;
}

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    std::string outDir = "/home/pedro/.gemini/antigravity-cli/brain/e5faaf39-9f30-404d-b05e-db381fb47f50";
    if (argc > 1) {
        outDir = argv[1];
    }

    // 1. Render Portrait Idle
    {
        _windowWidth = 1080;
        _windowHeight = 2400;
        g_scaleMode = 0; // FIT Handheld
        g_stickActive = false;
        g_androidTouchHeldMask = 0;
        updateLayout();

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, _windowWidth, _windowHeight, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);

        SDL_SetRenderDrawColor(r, 15, 18, 23, 255); // Obsidian Matte #0f1217
        SDL_RenderClear(r);

        drawMockPicoScreen(r);
        drawConsoleChassis(r);

        saveRendererToPng(r, _windowWidth, _windowHeight, outDir + "/chassis_portrait_idle.png");
        SDL_DestroyRenderer(r);
        SDL_FreeSurface(surf);
    }

    // 2. Render Portrait Active (Stick tilted Up-Right, Button O Pressed)
    {
        _windowWidth = 1080;
        _windowHeight = 2400;
        g_scaleMode = 0; // FIT Handheld
        g_stickActive = true;
        g_androidTouchHeldMask = P8_KEY_UP | P8_KEY_RIGHT | P8_KEY_O;
        updateLayout();

        // Deflect stick
        g_stickNubX = g_stickBaseX + (int)(g_stickRadius * 0.707f);
        g_stickNubY = g_stickBaseY - (int)(g_stickRadius * 0.707f);

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, _windowWidth, _windowHeight, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);

        SDL_SetRenderDrawColor(r, 15, 18, 23, 255);
        SDL_RenderClear(r);

        drawMockPicoScreen(r);
        drawConsoleChassis(r);

        saveRendererToPng(r, _windowWidth, _windowHeight, outDir + "/chassis_portrait_active.png");
        SDL_DestroyRenderer(r);
        SDL_FreeSurface(surf);
    }

    // 3. Render Landscape
    {
        _windowWidth = 2400;
        _windowHeight = 1080;
        g_scaleMode = 0;
        g_stickActive = false;
        g_androidTouchHeldMask = 0;
        updateLayout();

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, _windowWidth, _windowHeight, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);

        SDL_SetRenderDrawColor(r, 15, 18, 23, 255);
        SDL_RenderClear(r);

        drawMockPicoScreen(r);
        drawConsoleChassis(r);

        saveRendererToPng(r, _windowWidth, _windowHeight, outDir + "/chassis_landscape.png");
        SDL_DestroyRenderer(r);
        SDL_FreeSurface(surf);
    }

    // 4. Render Portrait 1:1 Pixel-Perfect (Mode 1)
    {
        _windowWidth = 1080;
        _windowHeight = 2400;
        g_scaleMode = 1;
        g_stickActive = false;
        g_androidTouchHeldMask = 0;
        updateLayout();

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, _windowWidth, _windowHeight, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);

        SDL_SetRenderDrawColor(r, 15, 18, 23, 255);
        SDL_RenderClear(r);

        drawMockPicoScreen(r);
        drawConsoleChassis(r);

        saveRendererToPng(r, _windowWidth, _windowHeight, outDir + "/chassis_portrait_mode1.png");
        SDL_DestroyRenderer(r);
        SDL_FreeSurface(surf);
    }

    // 5. Render Portrait MAX Edge-to-Edge (Mode 2)
    {
        _windowWidth = 1080;
        _windowHeight = 2400;
        g_scaleMode = 2;
        g_stickActive = false;
        g_androidTouchHeldMask = 0;
        updateLayout();

        SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, _windowWidth, _windowHeight, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* r = SDL_CreateSoftwareRenderer(surf);

        SDL_SetRenderDrawColor(r, 15, 18, 23, 255);
        SDL_RenderClear(r);

        drawMockPicoScreen(r);
        drawConsoleChassis(r);

        saveRendererToPng(r, _windowWidth, _windowHeight, outDir + "/chassis_portrait_mode2.png");
        SDL_DestroyRenderer(r);
        SDL_FreeSurface(surf);
    }

    SDL_Quit();
    return 0;
}
