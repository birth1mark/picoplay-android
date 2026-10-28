#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <algorithm>

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
SDL_Rect g_dpadRect = {0, 0, 0, 0};
SDL_Rect g_btnORect = {0, 0, 0, 0};
SDL_Rect g_btnXRect = {0, 0, 0, 0};
SDL_Rect g_btnMenuRect = {0, 0, 0, 0};
SDL_Rect g_btnImportRect = {0, 0, 0, 0};
SDL_Rect g_btnScaleRect = {0, 0, 0, 0};

// Compact 3x5 pixel font for retro touch button labels
static uint16_t getGlyph3x5(char c) {
    switch (c) {
        case 'A': return 0b010101111101101;
        case 'B': return 0b110101110101110;
        case 'C': return 0b011100100100011;
        case 'D': return 0b110101101101110;
        case 'E': return 0b111100110100111;
        case 'F': return 0b111100110100100;
        case 'G': return 0b011100101101011;
        case 'H': return 0b101101111101101;
        case 'I': return 0b111010010010111;
        case 'K': return 0b101101110101101;
        case 'L': return 0b100100100100111;
        case 'M': return 0b101111111101101;
        case 'N': return 0b111101101101101;
        case 'O': return 0b010101101101010;
        case 'P': return 0b110101110100100;
        case 'R': return 0b110101110101101;
        case 'S': return 0b011100010001110;
        case 'T': return 0b111010010010010;
        case 'U': return 0b101101101101111;
        case 'V': return 0b101101101101010;
        case 'X': return 0b101101010101101;
        case 'Y': return 0b101101010010010;
        case '+': return 0b000010111010000;
        default:  return 0;
    }
}

static void drawPixelText(SDL_Renderer* r, const char* text, int cx, int cy, int scale, Uint8 R, Uint8 G, Uint8 B) {
    int len = (int)strlen(text);
    int totalW = len * 4 * scale - scale;
    int startX = cx - totalW / 2;
    int startY = cy - (5 * scale) / 2;
    SDL_SetRenderDrawColor(r, R, G, B, 255);
    for (int i = 0; i < len; i++) {
        uint16_t g = getGlyph3x5(text[i]);
        int gx = startX + i * 4 * scale;
        for (int row = 0; row < 5; row++) {
            for (int col = 0; col < 3; col++) {
                if ((g >> (14 - (row * 3 + col))) & 1) {
                    SDL_Rect px = {gx + col * scale, startY + row * scale, scale, scale};
                    SDL_RenderFillRect(r, &px);
                }
            }
        }
    }
}

static void updateAndroidLayout() {
    int outW = _windowWidth, outH = _windowHeight;
    if (renderer && SDL_GetRendererOutputSize(renderer, &outW, &outH) == 0 && outW > 0 && outH > 0) {
        _windowWidth = outW;
        _windowHeight = outH;
    }

    bool portrait = (_windowHeight >= _windowWidth);
    if (portrait) {
        int topSafe = _windowHeight / 28;
        int topBarH = _windowHeight / 22;
        int btnW = (_windowWidth - 64) / 3;

        g_btnMenuRect   = {16, topSafe, btnW, topBarH};
        g_btnImportRect = {16 + btnW + 16, topSafe, btnW, topBarH};
        g_btnScaleRect  = {16 + (btnW + 16) * 2, topSafe, btnW, topBarH};

        int margin = _windowWidth / 24;
        int maxScreen = std::min(_windowWidth - margin * 2, (_windowHeight * 52) / 100);
        _screenWidth = maxScreen;
        _screenHeight = maxScreen;

        DestR.w = _screenWidth;
        DestR.h = _screenHeight;
        DestR.x = (_windowWidth - _screenWidth) / 2;
        DestR.y = topSafe + topBarH + (_windowHeight / 36);

        int controlsTop = DestR.y + DestR.h + (_windowHeight / 28);
        int controlsH = _windowHeight - controlsTop - (_windowHeight / 24);
        int dpadSize = std::min(_windowWidth * 44 / 100, std::max(180, controlsH * 78 / 100));
        int dpadX = _windowWidth * 7 / 100;
        int dpadY = controlsTop + (controlsH - dpadSize) / 2;
        g_dpadRect = {dpadX, dpadY, dpadSize, dpadSize};

        int btnSize = dpadSize * 40 / 100;
        int rightAreaX = _windowWidth * 54 / 100;
        int rightAreaW = _windowWidth * 40 / 100;
        g_btnORect = {rightAreaX + 8, dpadY + dpadSize - btnSize - 6, btnSize, btnSize};
        g_btnXRect = {rightAreaX + rightAreaW - btnSize - 4, dpadY + 12, btnSize, btnSize};
    } else {
        // Landscape layout
        int margin = _windowHeight / 24;
        int maxScreen = _windowHeight - margin * 2;
        _screenWidth = maxScreen;
        _screenHeight = maxScreen;
        DestR.w = _screenWidth;
        DestR.h = _screenHeight;
        DestR.x = (_windowWidth - _screenWidth) / 2;
        DestR.y = (_windowHeight - _screenHeight) / 2;

        int sideW = std::max(140, DestR.x - 16);
        int topBtnH = _windowHeight / 12;
        g_btnMenuRect   = {16, 20, sideW * 45 / 100, topBtnH};
        g_btnImportRect = {_windowWidth - sideW + 8, 20, sideW * 45 / 100, topBtnH};
        g_btnScaleRect  = {_windowWidth - (sideW * 45 / 100) - 16, 20, sideW * 45 / 100, topBtnH};

        int dpadSize = std::min(sideW - 16, _windowHeight * 52 / 100);
        g_dpadRect = {16 + (sideW - dpadSize) / 2, _windowHeight - dpadSize - 32, dpadSize, dpadSize};

        int btnSize = dpadSize * 42 / 100;
        int rightX = DestR.x + DestR.w + 12;
        g_btnORect = {rightX + 8, _windowHeight - btnSize - 36, btnSize, btnSize};
        g_btnXRect = {_windowWidth - btnSize - 20, _windowHeight - btnSize * 2 - 48, btnSize, btnSize};
    }
}

static void drawAndroidOverlay() {
    // Draw retro handheld bezel around DestR
    SDL_Rect bezel = {DestR.x - 8, DestR.y - 8, DestR.w + 16, DestR.h + 16};
    SDL_SetRenderDrawColor(renderer, 29, 43, 83, 255);
    SDL_RenderDrawRect(renderer, &bezel);
    SDL_Rect bezel2 = {DestR.x - 4, DestR.y - 4, DestR.w + 8, DestR.h + 8};
    SDL_SetRenderDrawColor(renderer, 95, 87, 79, 255);
    SDL_RenderDrawRect(renderer, &bezel2);

    if (!g_androidShowTouchControls) return;

    int fontScale = std::max(2, _windowWidth / 180);

    // Top bar pills: MENU, IMPORT, SCALE
    auto drawPill = [&](const SDL_Rect& r, const char* label, Uint8 bgR, Uint8 bgG, Uint8 bgB) {
        SDL_SetRenderDrawColor(renderer, bgR, bgG, bgB, 255);
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 194, 195, 199, 255);
        SDL_RenderDrawRect(renderer, &r);
        drawPixelText(renderer, label, r.x + r.w / 2, r.y + r.h / 2, fontScale, 255, 241, 232);
    };

    drawPill(g_btnMenuRect,   "MENU",   29, 43, 83);
    drawPill(g_btnImportRect, "+CART",  0, 135, 81);
    drawPill(g_btnScaleRect,  "SCALE",  95, 87, 79);

    // D-Pad Cross
    int armW = g_dpadRect.w / 3;
    int cx = g_dpadRect.x + armW;
    int cy = g_dpadRect.y + armW;

    SDL_Rect upArm    = {cx, g_dpadRect.y, armW, armW};
    SDL_Rect downArm  = {cx, cy + armW, armW, armW};
    SDL_Rect leftArm  = {g_dpadRect.x, cy, armW, armW};
    SDL_Rect rightArm = {cx + armW, cy, armW, armW};
    SDL_Rect centerHub = {cx, cy, armW, armW};

    auto drawArm = [&](const SDL_Rect& r, bool pressed, const char* sym) {
        if (pressed) {
            SDL_SetRenderDrawColor(renderer, 41, 173, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 45, 50, 68, 255);
        }
        SDL_RenderFillRect(renderer, &r);
        SDL_SetRenderDrawColor(renderer, 194, 195, 199, 255);
        SDL_RenderDrawRect(renderer, &r);
        drawPixelText(renderer, sym, r.x + r.w / 2, r.y + r.h / 2, fontScale + 1, 255, 241, 232);
    };

    SDL_SetRenderDrawColor(renderer, 32, 36, 50, 255);
    SDL_RenderFillRect(renderer, &centerHub);
    drawArm(upArm,    (g_androidTouchHeldMask & P8_KEY_UP) != 0,    "U");
    drawArm(downArm,  (g_androidTouchHeldMask & P8_KEY_DOWN) != 0,  "D");
    drawArm(leftArm,  (g_androidTouchHeldMask & P8_KEY_LEFT) != 0,  "L");
    drawArm(rightArm, (g_androidTouchHeldMask & P8_KEY_RIGHT) != 0, "R");

    // Action Buttons: O (Red #FF004D) and X (Blue #29ADFF)
    auto drawActionBtn = [&](const SDL_Rect& r, bool pressed, const char* label, Uint8 R, Uint8 G, Uint8 B) {
        SDL_Rect shadow = {r.x + 4, r.y + 6, r.w, r.h};
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderFillRect(renderer, &shadow);

        SDL_Rect body = pressed ? SDL_Rect{r.x + 2, r.y + 3, r.w, r.h} : r;
        if (pressed) {
            SDL_SetRenderDrawColor(renderer, 255, 236, 39, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, R, G, B, 255);
        }
        SDL_RenderFillRect(renderer, &body);
        SDL_SetRenderDrawColor(renderer, 255, 241, 232, 255);
        SDL_RenderDrawRect(renderer, &body);
        drawPixelText(renderer, label, body.x + body.w / 2, body.y + body.h / 2, fontScale + 3, pressed ? 0 : 255, pressed ? 0 : 241, pressed ? 0 : 232);
    };

    drawActionBtn(g_btnORect, (g_androidTouchHeldMask & P8_KEY_O) != 0, "O", 255, 0, 77);
    drawActionBtn(g_btnXRect, (g_androidTouchHeldMask & P8_KEY_X) != 0, "X", 41, 173, 255);
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
    want.samples = 512;  // Lower buffer size for reduced audio latency (~23ms vs ~186ms)
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
    frame_time = now_time - last_time;
	last_time = now_time;


	//sleep for remainder of time
	if (frame_time < targetFrameTimeMs) {
		uint32_t msToSleep = targetFrameTimeMs - frame_time;
        
        SDL_Delay(msToSleep);

		last_time += msToSleep;
	}
}


void Host::drawFrame(uint8_t* picoFb, uint8_t* screenPaletteMap, uint8_t drawMode){
    //clear screen to dark arcade background on Android, black elsewhere
#if defined(__ANDROID__)
    SDL_SetRenderDrawColor(renderer, 18, 20, 32, 255);
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
