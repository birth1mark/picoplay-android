#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <algorithm>
#include <vector>
#include <math.h>

#include <fstream>
#include <iostream>
using namespace std;

#include "sdl2basehost.h"
#include "../../../source/hostVmShared.h"
#include "../../../source/nibblehelpers.h"
#include "../../../source/logger.h"
#include "../../../source/filehelpers.h"

// sdl
#if defined(__ANDROID__)
#include <SDL.h>
#include <android/log.h>
#else
#include <SDL2/SDL.h>
#endif

#define SAMPLERATE 22050
#define SAMPLESPERBUF (SAMPLERATE / 30)
#define NUM_BUFFERS 2

int _windowWidth = 128;
int _windowHeight = 128;

int _screenWidth = 128;
int _screenHeight = 128;

int _maxNoStretchWidth = 128;
int _maxNoStretchHeight = 128;

const int PicoScreenWidth = 128;
const int PicoScreenHeight = 128;

uint32_t _windowFlags;
uint32_t _rendererFlags;
uint32_t _pixelFormat;

uint32_t last_time;
uint32_t now_time;
uint32_t frame_time;
uint32_t targetFrameTimeMs;

uint8_t currKDown;
uint8_t currKHeld;
bool stretchKeyPressed = false;

Audio* _audio;

SDL_Window* window;
SDL_Renderer *renderer;
SDL_Texture *texture = NULL;
SDL_AudioSpec want, have;
SDL_AudioDeviceID dev;
void *pixels;
uint8_t *base;
int pitch;

SDL_Rect DestR;
SDL_Rect SrcR;
double textureAngle;
SDL_RendererFlip flip;

int joystickCount = 0;

bool audioInitialized = false;

#if defined(__ANDROID__)
// Shared touch state updated by AndroidHost.cpp
bool g_androidShowTouchControls = true;
uint8_t g_androidTouchHeldMask = 0;
int g_scaleMode = 2; // 0 = Fit Handheld, 1 = Pixel Perfect, 2 = Fill Width (Default: MAX edge-to-edge)

// Virtual Transparent Thumb Joystick coordinates
int g_stickBaseX = 0;
int g_stickBaseY = 0;
int g_stickDefaultBaseX = 0;
int g_stickDefaultBaseY = 0;
int g_stickNubX = 0;
int g_stickNubY = 0;
int g_stickRadius = 80;
int g_stickNubRadius = 36;
bool g_stickActive = false;

// Circular Action Buttons: O (Red) and X (Blue)
int g_btnOCenterX = 0, g_btnOCenterY = 0, g_btnORadius = 45;
int g_btnXCenterX = 0, g_btnXCenterY = 0, g_btnXRadius = 45;

// Top bar pills (MENU, +CART, SCALE)
SDL_Rect g_btnMenuRect = {0, 0, 0, 0};
SDL_Rect g_btnImportRect = {0, 0, 0, 0};
SDL_Rect g_btnScaleRect = {0, 0, 0, 0};

// Geometry rendering helpers with alpha blending (batched for performance)
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

// Compact 3x5 pixel font for retro touch button labels
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
    renderCircleRing(r, cx, cy + 2, glyphR, thick, 0, 0, 0, 160);
    renderCircleRing(r, cx, cy, glyphR, thick, R, G, B, A);
}

static void drawBoldButtonXGlyph(SDL_Renderer* r, int cx, int cy, int radius, Uint8 R, Uint8 G, Uint8 B, Uint8 A = 255) {
    int armLen = radius * 35 / 100;
    int thick = std::max(3, radius * 13 / 100);

    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    for (int t = -thick / 2; t <= thick / 2; t++) {
        SDL_RenderDrawLine(r, cx - armLen + t, cy - armLen + 2, cx + armLen + t, cy + armLen + 2);
        SDL_RenderDrawLine(r, cx - armLen + t, cy + armLen + 2, cx + armLen + t, cy - armLen + 2);
    }
    SDL_SetRenderDrawColor(r, R, G, B, A);
    for (int t = -thick / 2; t <= thick / 2; t++) {
        SDL_RenderDrawLine(r, cx - armLen + t, cy - armLen, cx + armLen + t, cy + armLen);
        SDL_RenderDrawLine(r, cx - armLen + t, cy + armLen, cx + armLen + t, cy - armLen);
    }
}

static int s_lastLayoutW = 0;
static int s_lastLayoutH = 0;
static int s_lastLayoutScale = -1;

void updateAndroidLayout() {
    int outW = _windowWidth, outH = _windowHeight;
    if (renderer && SDL_GetRendererOutputSize(renderer, &outW, &outH) == 0 && outW > 0 && outH > 0) {
        _windowWidth = outW;
        _windowHeight = outH;
    }

    if (outW == s_lastLayoutW && outH == s_lastLayoutH && g_scaleMode == s_lastLayoutScale) {
        return;
    }
    s_lastLayoutW = outW;
    s_lastLayoutH = outH;
    s_lastLayoutScale = g_scaleMode;

    bool portrait = (_windowHeight >= _windowWidth);
    if (portrait) {
        // Safe inset from camera notch / top status bar: at least 60px or 5% of screen height
        int topSafe = std::max(60, _windowHeight * 5 / 100);
        int topBarH = std::max(48, _windowHeight * 4 / 100);
        int topBarBottom = topSafe + topBarH;

        int marginX = 20;
        int spacing = 14;
        int btnW = (_windowWidth - (marginX * 2) - (spacing * 2)) / 3;

        g_btnMenuRect   = {marginX, topSafe, btnW, topBarH};
        g_btnImportRect = {marginX + btnW + spacing, topSafe, btnW, topBarH};
        g_btnScaleRect  = {marginX + (btnW + spacing) * 2, topSafe, btnW, topBarH};

        // Dedicated Lower Control Deck: stable placement regardless of scaling mode
        int controlDeckY = _windowHeight * 55 / 100;
        int controlDeckH = _windowHeight - controlDeckY;

        // Thumb Virtual Joystick geometry (Lower Left)
        g_stickRadius = std::min(_windowWidth * 19 / 100, controlDeckH * 32 / 100);
        if (g_stickRadius < 65) g_stickRadius = 65;
        g_stickNubRadius = g_stickRadius * 46 / 100;
        g_stickBaseX = _windowWidth * 25 / 100;
        g_stickBaseY = controlDeckY + controlDeckH * 48 / 100;
        g_stickDefaultBaseX = g_stickBaseX;
        g_stickDefaultBaseY = g_stickBaseY;

        if (!g_stickActive) {
            g_stickNubX = g_stickBaseX;
            g_stickNubY = g_stickBaseY;
        }

        // Circular Action Buttons (Lower Right)
        g_btnORadius = g_stickRadius * 52 / 100;
        g_btnXRadius = g_btnORadius;

        // Ergonomic slanted placement for thumb
        int rightHubX = _windowWidth * 75 / 100;
        int rightHubY = g_stickBaseY;
        g_btnOCenterX = rightHubX - g_btnORadius - 8;
        g_btnOCenterY = rightHubY + g_btnORadius - 4;

        g_btnXCenterX = rightHubX + g_btnXRadius + 8;
        g_btnXCenterY = rightHubY - g_btnXRadius + 8;

        // Upper Display Viewport: mathematically centered between top bar and control deck
        int displayAreaTop = topBarBottom + 16;
        int displayAreaH = controlDeckY - displayAreaTop - 12;
        int maxBound = std::min(_windowWidth, displayAreaH);
        int maxIntegerFactor = maxBound / PicoScreenWidth;
        if (maxIntegerFactor < 1) maxIntegerFactor = 1;

        if (g_scaleMode == 1) {
            // Mode 1: 1:1 Pixel-Perfect maximum integer scaling (e.g. 8x = 1024px)
            _screenWidth = maxIntegerFactor * PicoScreenWidth;
            _screenHeight = maxIntegerFactor * PicoScreenHeight;
            DestR.w = _screenWidth;
            DestR.h = _screenHeight;
            DestR.x = (_windowWidth - _screenWidth) / 2;
            DestR.y = displayAreaTop + (displayAreaH - _screenHeight) / 2;
        } else if (g_scaleMode == 2) {
            // Mode 2: MAX Fill Width (1080px edge-to-edge)
            _screenWidth = _windowWidth;
            _screenHeight = _windowWidth;
            DestR.w = _screenWidth;
            DestR.h = _screenHeight;
            DestR.x = 0;
            DestR.y = displayAreaTop + (displayAreaH - _screenHeight) / 2;
            if (DestR.y < displayAreaTop) DestR.y = displayAreaTop;
        } else {
            // Mode 0: FIT Handheld Framed Display (1 integer step smaller with bezel, e.g. 7x = 896px)
            int fitFactor = std::max(1, maxIntegerFactor - 1);
            _screenWidth = fitFactor * PicoScreenWidth;
            _screenHeight = fitFactor * PicoScreenHeight;
            DestR.w = _screenWidth;
            DestR.h = _screenHeight;
            DestR.x = (_windowWidth - _screenWidth) / 2;
            DestR.y = displayAreaTop + (displayAreaH - _screenHeight) / 2;
        }
    } else {
        // Landscape layout
        int maxScreen = _windowHeight - 20;
        int maxInt = maxScreen / PicoScreenWidth;
        if (maxInt < 1) maxInt = 1;

        if (g_scaleMode == 1) {
            _screenWidth = maxInt * PicoScreenWidth;
            _screenHeight = maxInt * PicoScreenHeight;
        } else if (g_scaleMode == 2) {
            _screenWidth = _windowHeight; // full height edge-to-edge
            _screenHeight = _windowHeight;
        } else {
            int fitFactor = std::max(1, maxInt - 1);
            _screenWidth = fitFactor * PicoScreenWidth;
            _screenHeight = fitFactor * PicoScreenHeight;
        }
        DestR.w = _screenWidth;
        DestR.h = _screenHeight;
        DestR.x = (_windowWidth - _screenWidth) / 2;
        DestR.y = (_windowHeight - _screenHeight) / 2;

        int sideW = std::max(160, DestR.x - 16);
        int topBtnH = std::max(40, _windowHeight / 12);
        int topBtnW = sideW * 44 / 100;

        int topSafeLand = std::max(28, _windowHeight * 4 / 100);
        g_btnMenuRect   = {20, topSafeLand, topBtnW, topBtnH};
        g_btnImportRect = {20 + topBtnW + 12, topSafeLand, topBtnW, topBtnH};
        g_btnScaleRect  = {_windowWidth - topBtnW - 20, topSafeLand, topBtnW, topBtnH};

        // Virtual joystick on left margin
        g_stickRadius = std::min(sideW * 38 / 100, _windowHeight * 25 / 100);
        g_stickNubRadius = g_stickRadius * 46 / 100;
        g_stickBaseX = sideW / 2 + 10;
        g_stickBaseY = _windowHeight * 64 / 100;
        g_stickDefaultBaseX = g_stickBaseX;
        g_stickDefaultBaseY = g_stickBaseY;

        if (!g_stickActive) {
            g_stickNubX = g_stickBaseX;
            g_stickNubY = g_stickBaseY;
        }

        // Action buttons on right margin
        g_btnORadius = g_stickRadius * 52 / 100;
        g_btnXRadius = g_btnORadius;
        int rightBaseX = _windowWidth - sideW / 2 - 10;
        g_btnOCenterX = rightBaseX - g_btnORadius;
        g_btnOCenterY = _windowHeight * 68 / 100;
        g_btnXCenterX = rightBaseX + g_btnXRadius;
        g_btnXCenterY = _windowHeight * 54 / 100;
    }
}

static void drawAndroidOverlay() {
    if (g_stickBaseX <= 0 || g_stickDefaultBaseX <= 0) {
        updateAndroidLayout();
    }

    bool portrait = (_windowHeight >= _windowWidth);
    int controlDeckY = portrait ? (_windowHeight * 55 / 100) : 0;

    // 1. CHASSIS OUTER BEVEL & CNC CHAMFER
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    // Subtle top/left edge CNC highlight
    SDL_SetRenderDrawColor(renderer, 48, 56, 72, 160);
    SDL_RenderDrawLine(renderer, 0, 0, _windowWidth - 1, 0);
    SDL_RenderDrawLine(renderer, 0, 1, _windowWidth - 1, 1);
    SDL_RenderDrawLine(renderer, 0, 0, 0, _windowHeight - 1);
    SDL_RenderDrawLine(renderer, 1, 0, 1, _windowHeight - 1);

    // Subtle bottom/right edge CNC shadow
    SDL_SetRenderDrawColor(renderer, 6, 8, 11, 240);
    SDL_RenderDrawLine(renderer, 0, _windowHeight - 1, _windowWidth - 1, _windowHeight - 1);
    SDL_RenderDrawLine(renderer, 0, _windowHeight - 2, _windowWidth - 1, _windowHeight - 2);
    SDL_RenderDrawLine(renderer, _windowWidth - 1, 0, _windowWidth - 1, _windowHeight - 1);
    SDL_RenderDrawLine(renderer, _windowWidth - 2, 0, _windowWidth - 2, _windowHeight - 1);

    // 2. HARDWARE PARTING LINE / SEAM
    if (portrait) {
        int seamY = controlDeckY - 14;
        // Dark groove
        SDL_SetRenderDrawColor(renderer, 7, 9, 13, 255);
        SDL_RenderDrawLine(renderer, 12, seamY, _windowWidth - 13, seamY);
        // Bevel highlight below groove
        SDL_SetRenderDrawColor(renderer, 34, 40, 52, 170);
        SDL_RenderDrawLine(renderer, 12, seamY + 1, _windowWidth - 13, seamY + 1);
    } else {
        int leftSeamX = DestR.x - 14;
        int rightSeamX = DestR.x + DestR.w + 14;
        SDL_SetRenderDrawColor(renderer, 7, 9, 13, 255);
        SDL_RenderDrawLine(renderer, leftSeamX, 10, leftSeamX, _windowHeight - 11);
        SDL_RenderDrawLine(renderer, rightSeamX, 10, rightSeamX, _windowHeight - 11);
        SDL_SetRenderDrawColor(renderer, 34, 40, 52, 170);
        SDL_RenderDrawLine(renderer, leftSeamX + 1, 10, leftSeamX + 1, _windowHeight - 11);
        SDL_RenderDrawLine(renderer, rightSeamX + 1, 10, rightSeamX + 1, _windowHeight - 11);
    }

    // 3. DISPLAY FRAME & LAMINATED GLASS BEZEL
    if (DestR.x >= 6) {
        // Outer chamfered bezel
        SDL_Rect outerBezel = {DestR.x - 12, DestR.y - 12, DestR.w + 24, DestR.h + 24};
        // Top/left highlight
        SDL_SetRenderDrawColor(renderer, 38, 46, 60, 255);
        SDL_RenderDrawLine(renderer, outerBezel.x, outerBezel.y, outerBezel.x + outerBezel.w - 1, outerBezel.y);
        SDL_RenderDrawLine(renderer, outerBezel.x, outerBezel.y + 1, outerBezel.x + outerBezel.w - 2, outerBezel.y + 1);
        SDL_RenderDrawLine(renderer, outerBezel.x, outerBezel.y, outerBezel.x, outerBezel.y + outerBezel.h - 1);
        SDL_RenderDrawLine(renderer, outerBezel.x + 1, outerBezel.y, outerBezel.x + 1, outerBezel.y + outerBezel.h - 2);

        // Bottom/right shadow
        SDL_SetRenderDrawColor(renderer, 6, 8, 11, 255);
        SDL_RenderDrawLine(renderer, outerBezel.x, outerBezel.y + outerBezel.h - 1, outerBezel.x + outerBezel.w - 1, outerBezel.y + outerBezel.h - 1);
        SDL_RenderDrawLine(renderer, outerBezel.x + 1, outerBezel.y + outerBezel.h - 2, outerBezel.x + outerBezel.w - 1, outerBezel.y + outerBezel.h - 2);
        SDL_RenderDrawLine(renderer, outerBezel.x + outerBezel.w - 1, outerBezel.y, outerBezel.x + outerBezel.w - 1, outerBezel.y + outerBezel.h - 1);
        SDL_RenderDrawLine(renderer, outerBezel.x + outerBezel.w - 2, outerBezel.y + 1, outerBezel.x + outerBezel.w - 2, outerBezel.y + outerBezel.h - 1);

        // Piano black inner glass frame
        for (int i = 1; i <= 8; i++) {
            SDL_Rect glass = {DestR.x - i, DestR.y - i, DestR.w + i * 2, DestR.h + i * 2};
            SDL_SetRenderDrawColor(renderer, 7, 9, 12, 255);
            SDL_RenderDrawRect(renderer, &glass);
        }

        // Laminated glass top glare reflection line
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 48);
        SDL_RenderDrawLine(renderer, DestR.x - 4, DestR.y - 4, DestR.x + DestR.w + 3, DestR.y - 4);
    } else {
        // MAX Edge-to-edge mode: Top and Bottom precision integration seams
        SDL_SetRenderDrawColor(renderer, 7, 9, 12, 255);
        SDL_RenderDrawLine(renderer, 0, DestR.y - 1, _windowWidth - 1, DestR.y - 1);
        SDL_RenderDrawLine(renderer, 0, DestR.y + DestR.h, _windowWidth - 1, DestR.y + DestR.h);
        SDL_SetRenderDrawColor(renderer, 34, 40, 52, 160);
        SDL_RenderDrawLine(renderer, 0, DestR.y - 2, _windowWidth - 1, DestR.y - 2);
        SDL_RenderDrawLine(renderer, 0, DestR.y + DestR.h + 1, _windowWidth - 1, DestR.y + DestR.h + 1);
    }

    // 4. HARDWARE OPTICAL POWER / BATTERY LED
    int ledX = portrait ? (g_btnMenuRect.x + 8) : 24;
    int ledY = portrait ? (g_btnMenuRect.y - 22) : (g_btnMenuRect.y - 14);
    if (ledY < 14) ledY = 16;
    renderFilledCircle(renderer, ledX, ledY, 6, 8, 10, 14, 255);
    renderCircleRing(renderer, ledX, ledY, 6, 1, 30, 36, 48, 180);
    renderFilledCircle(renderer, ledX, ledY, 8, 0, 228, 54, 40);
    renderFilledCircle(renderer, ledX, ledY, 4, 0, 228, 54, 255);
    renderFilledCircle(renderer, ledX - 1, ledY - 1, 2, 210, 255, 230, 255);

    // 5. DEBOSSED HARDWARE LOGO: "P  I  C  O  P  L  A  Y"
    int logoScale = std::max(2, _windowWidth / 360);
    int logoX = _windowWidth / 2;
    int logoY = portrait ? (DestR.y + DestR.h + (controlDeckY - 14 - (DestR.y + DestR.h)) / 2) : (DestR.y >= 20 ? (DestR.y / 2) : (_windowHeight - 20));
    if ((portrait && logoY > DestR.y + DestR.h + 8) || (!portrait && logoY > 10)) {
        const char* brandText = "P I C O P L A Y";
        drawPixelText(renderer, brandText, logoX, logoY + 2, logoScale, 5, 7, 9, 240);
        drawPixelText(renderer, brandText, logoX, logoY, logoScale, 78, 88, 105, 210);
        drawPixelText(renderer, brandText, logoX, logoY - 1, logoScale, 110, 122, 142, 70);
    }

    // 6. PRECISION DRILLED SPEAKER PINHOLES
    int speakerBaseX = portrait ? (_windowWidth * 80 / 100) : (_windowWidth - 80);
    int speakerBaseY = portrait ? (_windowHeight - 48) : (_windowHeight - 40);
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 5; col++) {
            int hx = speakerBaseX + col * 13;
            int hy = speakerBaseY + row * 13;
            renderFilledCircle(renderer, hx, hy, 3, 5, 7, 10, 255);
            renderCircleRing(renderer, hx, hy + 1, 3, 1, 44, 52, 68, 170);
        }
    }

    if (!g_androidShowTouchControls) return;

    // 7. TOP RECESSED PILLS: [MENU], [+CART], [SCALE]
    int fontScale = std::max(2, _windowWidth / 220);
    auto drawRecessedPill = [&](const SDL_Rect& rect, const char* label, Uint8 bgR, Uint8 bgG, Uint8 bgB) {
        SDL_Rect well = {rect.x - 2, rect.y - 2, rect.w + 4, rect.h + 4};
        renderRoundedRect(renderer, well, 10, 7, 9, 12, 255);
        SDL_SetRenderDrawColor(renderer, 34, 40, 52, 180);
        SDL_RenderDrawLine(renderer, well.x + 8, well.y + well.h - 1, well.x + well.w - 8, well.y + well.h - 1);

        renderRoundedRect(renderer, rect, 8, bgR, bgG, bgB, 240);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 60);
        SDL_RenderDrawLine(renderer, rect.x + 8, rect.y + 1, rect.x + rect.w - 8, rect.y + 1);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 90);
        SDL_RenderDrawLine(renderer, rect.x + 8, rect.y + rect.h - 2, rect.x + rect.w - 8, rect.y + rect.h - 2);

        drawPixelText(renderer, label, rect.x + rect.w / 2, rect.y + rect.h / 2 + 1, fontScale, 0, 0, 0, 180);
        drawPixelText(renderer, label, rect.x + rect.w / 2, rect.y + rect.h / 2, fontScale, 240, 245, 255, 240);
    };

    drawRecessedPill(g_btnMenuRect,   "MENU",  24, 28, 38);
    drawRecessedPill(g_btnImportRect, "+CART", 12, 72, 48);
    const char* scaleLabel = (g_scaleMode == 1) ? "1:1" : ((g_scaleMode == 2) ? "MAX" : "FIT");
    drawRecessedPill(g_btnScaleRect,  scaleLabel, 34, 40, 52);

    // 8. MACHINED D-PAD / THUMBSTICK RECESSED DISH
    int wellR = g_stickRadius + 14;
    renderFilledCircle(renderer, g_stickBaseX, g_stickBaseY, wellR, 8, 10, 13, 255);
    renderCircleRing(renderer, g_stickBaseX, g_stickBaseY, wellR, 2, 34, 40, 52, 190);

    renderFilledCircle(renderer, g_stickBaseX, g_stickBaseY, g_stickRadius + 4, 13, 16, 21, 255);
    renderCircleRing(renderer, g_stickBaseX, g_stickBaseY, g_stickRadius + 4, 1, 20, 24, 32, 180);
    renderFilledCircle(renderer, g_stickBaseX, g_stickBaseY, g_stickRadius - 8, 10, 13, 17, 255);

    // Cardinal Directional Engraved Arrows (▲, ▼, ◀, ▶)
    int markerDist = g_stickRadius - 6;
    int arrowW = std::max(6, g_stickRadius * 12 / 100);
    int arrowH = std::max(5, g_stickRadius * 10 / 100);

    // UP Arrow
    bool upHeld = (g_androidTouchHeldMask & P8_KEY_UP) != 0;
    if (upHeld) {
        renderFilledCircle(renderer, g_stickBaseX, g_stickBaseY - markerDist, 10, 41, 173, 255, 90);
        renderFilledTriangle(renderer, g_stickBaseX, g_stickBaseY - markerDist - arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY - markerDist + arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY - markerDist + arrowH,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(renderer, g_stickBaseX, g_stickBaseY - markerDist - arrowH + 1,
                                g_stickBaseX - arrowW, g_stickBaseY - markerDist + arrowH + 1,
                                g_stickBaseX + arrowW, g_stickBaseY - markerDist + arrowH + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(renderer, g_stickBaseX, g_stickBaseY - markerDist - arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY - markerDist + arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY - markerDist + arrowH,
                                60, 70, 86, 200);
    }

    // DOWN Arrow
    bool downHeld = (g_androidTouchHeldMask & P8_KEY_DOWN) != 0;
    if (downHeld) {
        renderFilledCircle(renderer, g_stickBaseX, g_stickBaseY + markerDist, 10, 41, 173, 255, 90);
        renderFilledTriangle(renderer, g_stickBaseX, g_stickBaseY + markerDist + arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY + markerDist - arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY + markerDist - arrowH,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(renderer, g_stickBaseX, g_stickBaseY + markerDist + arrowH + 1,
                                g_stickBaseX - arrowW, g_stickBaseY + markerDist - arrowH + 1,
                                g_stickBaseX + arrowW, g_stickBaseY + markerDist - arrowH + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(renderer, g_stickBaseX, g_stickBaseY + markerDist + arrowH,
                                g_stickBaseX - arrowW, g_stickBaseY + markerDist - arrowH,
                                g_stickBaseX + arrowW, g_stickBaseY + markerDist - arrowH,
                                60, 70, 86, 200);
    }

    // LEFT Arrow
    bool leftHeld = (g_androidTouchHeldMask & P8_KEY_LEFT) != 0;
    if (leftHeld) {
        renderFilledCircle(renderer, g_stickBaseX - markerDist, g_stickBaseY, 10, 41, 173, 255, 90);
        renderFilledTriangle(renderer, g_stickBaseX - markerDist - arrowH, g_stickBaseY,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY + arrowW,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(renderer, g_stickBaseX - markerDist - arrowH + 1, g_stickBaseY + 1,
                                g_stickBaseX - markerDist + arrowH + 1, g_stickBaseY - arrowW + 1,
                                g_stickBaseX - markerDist + arrowH + 1, g_stickBaseY + arrowW + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(renderer, g_stickBaseX - markerDist - arrowH, g_stickBaseY,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX - markerDist + arrowH, g_stickBaseY + arrowW,
                                60, 70, 86, 200);
    }

    // RIGHT Arrow
    bool rightHeld = (g_androidTouchHeldMask & P8_KEY_RIGHT) != 0;
    if (rightHeld) {
        renderFilledCircle(renderer, g_stickBaseX + markerDist, g_stickBaseY, 10, 41, 173, 255, 90);
        renderFilledTriangle(renderer, g_stickBaseX + markerDist + arrowH, g_stickBaseY,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY + arrowW,
                                255, 255, 255, 255);
    } else {
        renderFilledTriangle(renderer, g_stickBaseX + markerDist + arrowH + 1, g_stickBaseY + 1,
                                g_stickBaseX + markerDist - arrowH + 1, g_stickBaseY - arrowW + 1,
                                g_stickBaseX + markerDist - arrowH + 1, g_stickBaseY + arrowW + 1,
                                4, 5, 7, 240);
        renderFilledTriangle(renderer, g_stickBaseX + markerDist + arrowH, g_stickBaseY,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY - arrowW,
                                g_stickBaseX + markerDist - arrowH, g_stickBaseY + arrowW,
                                60, 70, 86, 200);
    }

    // Tactile Machined Thumb Nub
    Uint8 nubFillR = g_stickActive ? 32 : 24;
    Uint8 nubFillG = g_stickActive ? 38 : 28;
    Uint8 nubFillB = g_stickActive ? 52 : 38;

    if (!g_stickActive) {
        renderFilledCircle(renderer, g_stickNubX + 2, g_stickNubY + 5, g_stickNubRadius, 4, 5, 7, 190);
    } else {
        renderFilledCircle(renderer, g_stickNubX, g_stickNubY, g_stickNubRadius + 6, 41, 173, 255, 60);
    }
    renderFilledCircle(renderer, g_stickNubX, g_stickNubY, g_stickNubRadius, nubFillR, nubFillG, nubFillB, 255);
    renderCircleRing(renderer, g_stickNubX, g_stickNubY, g_stickNubRadius, 3, 58, 68, 86, 220);

    // Radial knurling ticks around the perimeter (8 notch ticks)
    for (int a = 0; a < 8; a++) {
        float angle = a * (3.14159265f / 4.0f);
        int tx0 = g_stickNubX + (int)(cosf(angle) * (g_stickNubRadius - 3));
        int ty0 = g_stickNubY + (int)(sinf(angle) * (g_stickNubRadius - 3));
        int tx1 = g_stickNubX + (int)(cosf(angle) * (g_stickNubRadius + 1));
        int ty1 = g_stickNubY + (int)(sinf(angle) * (g_stickNubRadius + 1));
        SDL_SetRenderDrawColor(renderer, 30, 36, 46, 255);
        SDL_RenderDrawLine(renderer, tx0, ty0, tx1, ty1);
    }

    renderCircleRing(renderer, g_stickNubX - 1, g_stickNubY - 1, g_stickNubRadius - 2, 1, 255, 255, 255, g_stickActive ? 180 : 90);
    renderFilledCircle(renderer, g_stickNubX, g_stickNubY, g_stickNubRadius * 48 / 100, 14, 16, 22, 255);
    renderCircleRing(renderer, g_stickNubX, g_stickNubY, g_stickNubRadius * 48 / 100, 1, 8, 10, 14, 255);
    renderFilledCircle(renderer, g_stickNubX, g_stickNubY, 3, 44, 52, 68, 240);

    // 9. 3D MECHANICAL ACTION BUTTONS ([O] and [X])
    auto drawMechanicalButton = [&](int cx, int cy, int radius, bool pressed, bool isO, Uint8 baseR, Uint8 baseG, Uint8 baseB) {
        int wellRadius = radius + 9;
        renderFilledCircle(renderer, cx, cy, wellRadius, 7, 9, 12, 255);
        renderCircleRing(renderer, cx, cy, wellRadius, 2, 4, 5, 7, 240);
        renderCircleRing(renderer, cx, cy + 1, wellRadius, 1, 34, 40, 52, 190);

        int capY = pressed ? (cy + 4) : cy;
        int capX = cx;

        if (!pressed) {
            renderFilledCircle(renderer, capX, capY + 5, radius, 3, 4, 6, 210);
        } else {
            renderFilledCircle(renderer, capX, capY, radius + 5, baseR, baseG, baseB, 75);
        }

        Uint8 curR = pressed ? std::min(255, baseR + 35) : baseR;
        Uint8 curG = pressed ? std::min(255, baseG + 35) : baseG;
        Uint8 curB = pressed ? std::min(255, baseB + 35) : baseB;
        renderFilledCircle(renderer, capX, capY, radius, curR, curG, curB, 255);

        Uint8 rimR = pressed ? 255 : (Uint8)(baseR * 0.72f);
        Uint8 rimG = pressed ? 255 : (Uint8)(baseG * 0.72f);
        Uint8 rimB = pressed ? 255 : (Uint8)(baseB * 0.72f);
        renderCircleRing(renderer, capX, capY, radius, 2, rimR, rimG, rimB, 230);

        if (!pressed) {
            renderCircleRing(renderer, capX - 1, capY - 1, radius - 2, 2, 255, 255, 255, 95);
        }

        if (isO) {
            drawBoldButtonOGlyph(renderer, capX, capY, radius, 255, 255, 255, 255);
        } else {
            drawBoldButtonXGlyph(renderer, capX, capY, radius, 255, 255, 255, 255);
        }
    };

    drawMechanicalButton(g_btnOCenterX, g_btnOCenterY, g_btnORadius, (g_androidTouchHeldMask & P8_KEY_O) != 0, true, 255, 0, 77);
    drawMechanicalButton(g_btnXCenterX, g_btnXCenterY, g_btnXRadius, (g_androidTouchHeldMask & P8_KEY_X) != 0, false, 41, 173, 255);
}
#endif

void postFlipFunction(){
    // We're done rendering, so we end the frame here.
    SDL_UnlockTexture(texture);
#if defined(__ANDROID__)
    updateAndroidLayout();
#endif
    SDL_RenderCopyEx(renderer, texture, &SrcR, &DestR, textureAngle, NULL, flip);

#if defined(__ANDROID__)
    drawAndroidOverlay();
#endif

    SDL_RenderPresent(renderer);
}

void audioCleanup(){
    audioInitialized = false;

    SDL_CloseAudioDevice(dev);
}


void FillAudioDeviceBuffer(void* UserData, Uint8* DeviceBuffer, int Length)
{
    _audio->FillAudioBuffer(DeviceBuffer, 0, Length / 4);
}

void audioSetup(){
    //modifed from SDL docs: https://wiki.libsdl.org/SDL_OpenAudioDevice

    SDL_memset(&want, 0, sizeof(want)); /* or SDL_zero(want) */
    want.freq = SAMPLERATE;
    want.format = AUDIO_S16;
    want.channels = 2;
#if defined(__ANDROID__)
    want.samples = 1024;  // 1024 samples (~46ms buffer) prevents AAudio underruns and crackling on Android
#else
    want.samples = 512;   // Lower buffer size for reduced audio latency on Desktop / PS4
#endif
    want.callback = FillAudioDeviceBuffer;
    

    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        Logger_Write("Failed to open audio: %s", SDL_GetError());
    } else {
        if (have.format != want.format) { /* we let this one thing change. */
            Logger_Write("We didn't get requested audio format.");
        }
        SDL_PauseAudioDevice(dev, 0); /* start audio playing. */
        audioInitialized = true;
    }
}

void _changeStretch(StretchOption newStretch){
#if defined(__ANDROID__)
    updateAndroidLayout();
    SrcR.x = 0;
    SrcR.y = 0;
    SrcR.w = PicoScreenWidth;
    SrcR.h = PicoScreenHeight;
    textureAngle = 0;
    flip = SDL_FLIP_NONE;
    return;
#endif
    if (newStretch == PixelPerfect) {
        _screenWidth = PicoScreenWidth;
        _screenHeight = PicoScreenHeight;
    }
    else if (newStretch == StretchToFit) {
        int fitDim = std::min(_windowWidth, _windowHeight);
        _screenWidth = fitDim;
        _screenHeight = fitDim;
    }
    else if (newStretch == StretchToFill){
        _screenWidth = _windowWidth;
        _screenHeight = _windowHeight; 
    }
    else if (newStretch == PixelPerfectStretch) {
        int factor = std::min(_windowWidth / PicoScreenWidth, _windowHeight / PicoScreenHeight);
        if (factor < 1) factor = 1;
        _screenWidth = factor * PicoScreenWidth;
        _screenHeight = factor * PicoScreenHeight; 
    }
    else if (newStretch == FourByThreeVertPerfect) {
        _screenWidth = _maxNoStretchHeight * 4 / 3;
        _screenHeight = _maxNoStretchHeight; 
    }
    else if (newStretch == FourByThreeStretch) {
        _screenWidth = _windowHeight * 4 / 3;
        _screenHeight = _windowHeight; 
    }
    

    DestR.x = (_windowWidth - _screenWidth) / 2;
    DestR.y = (_windowHeight - _screenHeight) / 2;
    DestR.w = _screenWidth;
    DestR.h = _screenHeight;

    SrcR.x = 0;
    SrcR.y = 0;
    SrcR.w = PicoScreenWidth;
    SrcR.h = PicoScreenHeight;

    textureAngle = 0;
    flip = SDL_FLIP_NONE;
}

void Host::setPlatformParams(
    int windowWidth,
    int windowHeight,
    uint32_t sdlWindowFlags,
    uint32_t sdlRendererFlags,
    uint32_t sdlPixelFormat,
    std::string logFilePrefix,
    std::string customBiosLua,
    std::string cartDirectory) 
{
    _windowWidth = windowWidth;
    _windowHeight = windowHeight;
    
    //assume wide screen, height is limiting factor
    int maxNoStretchFactor = _windowHeight / PicoScreenHeight;

    _maxNoStretchWidth = maxNoStretchFactor * PicoScreenWidth;
    _maxNoStretchHeight = maxNoStretchFactor * PicoScreenHeight;

    _screenWidth = _maxNoStretchWidth;
    _screenHeight = _maxNoStretchHeight;

    _windowFlags = sdlWindowFlags;
    _rendererFlags = sdlRendererFlags;
    _pixelFormat = sdlPixelFormat;

    _logFilePrefix = logFilePrefix;
    _customBiosLua = customBiosLua;
    _cartDirectory = cartDirectory;

}


void Host::oneTimeSetup(Audio* audio){
#if defined(__ANDROID__)
    SDL_SetHint(SDL_HINT_AUDIODRIVER, "opensles");
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
    SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
#endif
#if defined(__PS4__) || defined(__ORBIS__)
    uint32_t initFlags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_TIMER | SDL_INIT_EVENTS;
#else
    uint32_t initFlags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER | SDL_INIT_EVENTS;
#endif
    if (SDL_Init(initFlags) != 0)
    {
        Logger_Write("SDL could not initialize: %s\n", SDL_GetError());
        return;
    }

#if defined(__PS4__) || defined(__ORBIS__) || defined(__ANDROID__)
    window = SDL_CreateWindow("PicoPlay", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _windowWidth, _windowHeight, _windowFlags);
#else
    window = SDL_CreateWindow("PicoPlay", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, _windowWidth, _windowHeight, _windowFlags);
#endif
	if (!window) 
    { 
        Logger_Write("Error creating window: %s\n", SDL_GetError());
        quit = 1;
        return; 
    }
	
	renderer = SDL_CreateRenderer(window, -1, _rendererFlags);
    if (!renderer)
    {
        SDL_Surface* windowSurface = SDL_GetWindowSurface(window);
        if (windowSurface) {
            renderer = SDL_CreateSoftwareRenderer(windowSurface);
        }
    }
	if (!renderer) 
    { 
        Logger_Write("Error creating renderer: %s\n", SDL_GetError());
        quit = 1;
        return;
    }

#if defined(__ANDROID__)
    updateAndroidLayout();
#endif

    texture = SDL_CreateTexture(renderer, _pixelFormat, SDL_TEXTUREACCESS_STREAMING, PicoScreenWidth, PicoScreenHeight);
    if (!texture)
    {
        Logger_Write("Error creating texture: %s\n", SDL_GetError());
        quit = 1;
        return;
    }

    atexit(SDL_Quit);

    _audio = audio;
    audioSetup();

    joystickCount = SDL_NumJoysticks();
    for (int i = 0; i < joystickCount; i++) {
		if (SDL_JoystickOpen(i) == NULL) {
			Logger_Write("Failed to open joystick %d: %s\n", i, SDL_GetError());
		}
    }

    last_time = 0;
    now_time = 0;
    frame_time = 0;
    targetFrameTimeMs = 0;

    currKDown = 0;
    currKHeld = 0;

    loadSettingsIni();

    _changeStretch(stretch);

    scaleX = _screenWidth / (float)PicoScreenWidth;
    scaleY = _screenHeight / (float)PicoScreenHeight;
    mouseOffsetX = DestR.x;
    mouseOffsetY = DestR.y;
}

void Host::oneTimeCleanup(){
    saveSettingsIni();

    audioCleanup();

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void Host::setTargetFps(int targetFps){
    targetFrameTimeMs = 1000 / targetFps;
}

void Host::changeStretch(){
#if defined(__ANDROID__)
    if (::stretchKeyPressed || stretchKeyPressed) {
        ::stretchKeyPressed = false;
        stretchKeyPressed = false;
        g_scaleMode = (g_scaleMode + 1) % 3;
        updateAndroidLayout();
    }
    return;
#endif
    if (stretchKeyPressed && resizekey == YesResize) {
        StretchOption newStretch = stretch;

        if (stretch == PixelPerfectStretch) {
            newStretch = PixelPerfect;
        }
        else if (stretch == PixelPerfect) {
            newStretch = StretchToFit;
        }
        else if (stretch == StretchToFit) {
            newStretch = StretchToFill;
        }
        else if (stretch == StretchToFill) {
            newStretch = FourByThreeVertPerfect;
        }
        else if (stretch == FourByThreeVertPerfect) {
            newStretch = FourByThreeStretch;
        }
        else if (stretch == FourByThreeStretch) {
            newStretch = PixelPerfectStretch;
        }

        _changeStretch(newStretch);

        stretch = newStretch;
        scaleX = _screenWidth / (float)PicoScreenWidth;
        scaleY = _screenHeight / (float)PicoScreenHeight;
        mouseOffsetX = DestR.x;
        mouseOffsetY = DestR.y;
    }
}

void Host::forceStretch(StretchOption newStretch) {
	_changeStretch(newStretch);
	stretch = newStretch;
	scaleX = _screenWidth / (float)PicoScreenWidth;
	scaleY = _screenHeight / (float)PicoScreenHeight;
	mouseOffsetX = DestR.x;
	mouseOffsetY = DestR.y;
}

void Host::onWindowResized(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) return;

    _windowWidth = newWidth;
    _windowHeight = newHeight;

    int factor = std::min(_windowWidth / PicoScreenWidth, _windowHeight / PicoScreenHeight);
    if (factor < 1) factor = 1;

    _maxNoStretchWidth = factor * PicoScreenWidth;
    _maxNoStretchHeight = factor * PicoScreenHeight;

    _changeStretch(stretch);

    scaleX = _screenWidth / (float)PicoScreenWidth;
    scaleY = _screenHeight / (float)PicoScreenHeight;
    mouseOffsetX = DestR.x;
    mouseOffsetY = DestR.y;
}

bool Host::shouldQuit() {
    return quit == 1;
}

void Host::waitForTargetFps(){
    now_time = SDL_GetTicks();
    if (last_time == 0) {
        last_time = now_time;
        return;
    }
    frame_time = now_time - last_time;

    // Smooth frame pacing: avoid sleeping tiny durations (< 3ms) that clash with VSYNC
    if (frame_time < targetFrameTimeMs) {
        uint32_t msToSleep = targetFrameTimeMs - frame_time;
        if (msToSleep >= 3) {
            // Wake up slightly before VSYNC deadline to avoid dropping frames on high refresh displays
            SDL_Delay(msToSleep - 1);
        }
    }
    last_time = SDL_GetTicks();
}


void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode){
    //clear screen to obsidian matte chassis background on Android, black elsewhere
#if defined(__ANDROID__)
    SDL_SetRenderDrawColor(renderer, 15, 18, 23, 255);
#else
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
#endif
    SDL_RenderClear(renderer);

    SDL_LockTexture(texture, NULL, &pixels, &pitch);

    for (int y = 0; y < PicoScreenHeight; y ++){
        for (int x = 0; x < PicoScreenWidth; x ++){
            uint8_t c = getPixelNibble(x, y, picoFb);
            // Apply 0x8F mask per PICO-8 spec: system color indices are masked with 0x8F
            // This maps values like 149 to valid palette indices (149 & 0x8F = 133)
            Color col = _paletteColors[screenPaletteMap[c] & 0x8f];

            base = ((Uint8 *)pixels) + (4 * ( y * PicoScreenHeight + x));
            base[0] = col.Blue;
            base[1] = col.Green;
            base[2] = col.Red;
            base[3] = col.Alpha;
        }
    }

    SrcR.x = 0;
    SrcR.y = 0;

    switch(drawMode){
        case 1:
            SrcR.w = 64;
            SrcR.h = PicoScreenHeight;
            textureAngle = 0;
            flip = SDL_FLIP_NONE;
            break;
        case 2:
            SrcR.w = PicoScreenWidth;
            SrcR.h = 64;
            textureAngle = 0;
            flip = SDL_FLIP_NONE;
            break;
        case 3:
            SrcR.w = 64;
            SrcR.h = 64;
            textureAngle = 0;
            flip = SDL_FLIP_NONE;
            break;
        case 129:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 0;
            flip = SDL_FLIP_HORIZONTAL;
            break;
        case 130:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 0;
            flip = SDL_FLIP_VERTICAL;
            break;
        case 131:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 0;
            flip = (SDL_RendererFlip)(SDL_FLIP_HORIZONTAL | SDL_FLIP_VERTICAL);
            break;
        case 133:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 90;
            flip = SDL_FLIP_NONE;
            break;
        case 134:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 180;
            flip = SDL_FLIP_NONE;
            break;
        case 135:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 270;
            flip = SDL_FLIP_NONE;
            break;
        default:
            SrcR.w = PicoScreenWidth;
            SrcR.h = PicoScreenHeight;
            textureAngle = 0;
            flip = SDL_FLIP_NONE;
            break;
    }
    

    postFlipFunction();
}

bool Host::shouldFillAudioBuff(){
    return false;
}

void* Host::getAudioBufferPointer(){
    return nullptr;
}

size_t Host::getAudioBufferSize(){
    return 0;
}

void Host::playFilledAudioBuffer(){
}

bool Host::shouldRunMainLoop(){
    if (shouldQuit()){
        return false;
    }

    return true;
}

const char* Host::logFilePrefix() {
    return _logFilePrefix.c_str();
}

std::string Host::customBiosLua() {
    return _customBiosLua;
}

std::string Host::getCartDirectory() {
    return _cartDirectory;
}
