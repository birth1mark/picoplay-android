#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>

#include "../../../source/host.h"
#include "../../../source/hostVmShared.h"
#include "../../../source/filehelpers.h"
#include "../../../source/logger.h"

#include <SDL.h>
#include <jni.h>

extern int _windowWidth;
extern int _windowHeight;
extern uint8_t currKDown;
extern uint8_t currKHeld;
extern bool stretchKeyPressed;
extern SDL_Rect DestR;
extern SDL_Renderer *renderer;

extern bool g_androidShowTouchControls;
extern uint8_t g_androidTouchHeldMask;
extern int g_scaleMode;

extern int g_stickBaseX;
extern int g_stickBaseY;
extern int g_stickNubX;
extern int g_stickNubY;
extern int g_stickRadius;
extern int g_stickNubRadius;
extern bool g_stickActive;

extern int g_btnOCenterX, g_btnOCenterY, g_btnORadius;
extern int g_btnXCenterX, g_btnXCenterY, g_btnXRadius;

extern SDL_Rect g_btnMenuRect;
extern SDL_Rect g_btnImportRect;
extern SDL_Rect g_btnScaleRect;

static uint8_t s_prevCombinedHeld = 0;
static uint8_t s_joyHeldMask = 0;
static bool s_prevMenuTap = false;
static bool s_prevImportTap = false;
static bool s_prevScaleTap = false;

static void triggerAndroidImportPicker() {
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (!env) return;
    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (!activity) return;
    jclass cls = env->GetObjectClass(activity);
    jmethodID mid = env->GetStaticMethodID(cls, "openImportCartridgePicker", "()V");
    if (mid) {
        env->CallStaticVoidMethod(cls, mid);
    }
    env->DeleteLocalRef(cls);
    env->DeleteLocalRef(activity);
}

static bool pointInExpandedRect(int px, int py, const SDL_Rect& r, int pad) {
    if (r.w <= 0 || r.h <= 0) return false;
    return (px >= r.x - pad && px <= r.x + r.w + pad &&
            py >= r.y - pad && py <= r.y + r.h + pad);
}

static bool pointInCircle(int px, int py, int cx, int cy, int radius, int pad) {
    if (radius <= 0) return false;
    int dx = px - cx;
    int dy = py - cy;
    int maxR = radius + pad;
    return (dx * dx + dy * dy) <= (maxR * maxR);
}

Host::Host(int windowWidth, int windowHeight)
{
    const char* internalPath = SDL_AndroidGetInternalStoragePath();
    std::string baseDir = internalPath ? std::string(internalPath) : "/data/data/com.birth1mark.picoplay/files";
    mkdir(baseDir.c_str(), 0777);

    std::string cartsDir = baseDir + "/carts";
    mkdir(cartsDir.c_str(), 0777);

    std::string cdataDir = baseDir + "/cdata";
    mkdir(cdataDir.c_str(), 0777);

    std::string logPrefix = baseDir + "/";
    std::string customBiosLua =
        "cartpath = \"" + cartsDir + "/\"\n"
        "selectbtn = \"x / o\"\n"
        "pausebtn = \"menu / back\"\n"
        "exitbtn = \"home\"\n"
        "sizebtn = \"scale button\"\n";

    uint32_t sdlWindowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN | SDL_WINDOW_RESIZABLE;
    uint32_t sdlRendererFlags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;
    uint32_t sdlPixelFormat = SDL_PIXELFORMAT_ARGB8888;

    if (windowWidth <= 0) windowWidth = 1080;
    if (windowHeight <= 0) windowHeight = 2400;

    setPlatformParams(
        windowWidth,
        windowHeight,
        sdlWindowFlags,
        sdlRendererFlags,
        sdlPixelFormat,
        logPrefix,
        customBiosLua,
        cartsDir
    );

    // Auto-detect if a physical controller (gamepad/joystick) or physical keyboard is present
    bool hasPhysical = false;
    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (env) {
        jobject activity = (jobject)SDL_AndroidGetActivity();
        if (activity) {
            jclass cls = env->GetObjectClass(activity);
            jmethodID mid = env->GetStaticMethodID(cls, "hasPhysicalGamepadOrKeyboard", "()Z");
            if (mid) {
                hasPhysical = env->CallStaticBooleanMethod(cls, mid);
            }
            env->DeleteLocalRef(cls);
            env->DeleteLocalRef(activity);
        }
    }
    // If physical controller/keyboard is connected, start with touch overlay hidden
    g_androidShowTouchControls = !hasPhysical;
}

InputState_t Host::scanInput() {
    uint8_t kDown = 0;
    uint8_t mouseBtnState = 0;
    int mouseX = 0, mouseY = 0;
    stretchKeyPressed = false;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                quit = 1;
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED ||
                    event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                    onWindowResized(event.window.data1, event.window.data2);
                }
                break;

            case SDL_FINGERDOWN:
                g_androidShowTouchControls = true;
                s_joyHeldMask = 0;
                currKHeld = 0;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_AC_BACK || event.key.keysym.sym == SDLK_ESCAPE) {
                    kDown |= P8_KEY_PAUSE;
                } else {
                    g_androidShowTouchControls = false;
                    switch (event.key.keysym.sym) {
                        case SDLK_LEFT:  currKHeld |= P8_KEY_LEFT; break;
                        case SDLK_RIGHT: currKHeld |= P8_KEY_RIGHT; break;
                        case SDLK_UP:    currKHeld |= P8_KEY_UP; break;
                        case SDLK_DOWN:  currKHeld |= P8_KEY_DOWN; break;
                        case SDLK_z: case SDLK_c: case SDLK_n: currKHeld |= P8_KEY_O; break;
                        case SDLK_x: case SDLK_v: case SDLK_m: currKHeld |= P8_KEY_X; break;
                    }
                }
                break;

            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_LEFT:  currKHeld &= ~P8_KEY_LEFT; break;
                    case SDLK_RIGHT: currKHeld &= ~P8_KEY_RIGHT; break;
                    case SDLK_UP:    currKHeld &= ~P8_KEY_UP; break;
                    case SDLK_DOWN:  currKHeld &= ~P8_KEY_DOWN; break;
                    case SDLK_z: case SDLK_c: case SDLK_n: currKHeld &= ~P8_KEY_O; break;
                    case SDLK_x: case SDLK_v: case SDLK_m: currKHeld &= ~P8_KEY_X; break;
                }
                break;

            case SDL_JOYDEVICEADDED:
                SDL_JoystickOpen(event.jdevice.which);
                break;

            case SDL_JOYBUTTONDOWN:
                g_androidShowTouchControls = false;
                if (event.jbutton.button == 0 || event.jbutton.button == 2) s_joyHeldMask |= P8_KEY_X;
                else if (event.jbutton.button == 1 || event.jbutton.button == 3) s_joyHeldMask |= P8_KEY_O;
                else if (event.jbutton.button == 6 || event.jbutton.button == 7 || event.jbutton.button == 9 || event.jbutton.button == 10) kDown |= P8_KEY_PAUSE;
                break;

            case SDL_JOYBUTTONUP:
                if (event.jbutton.button == 0 || event.jbutton.button == 2) s_joyHeldMask &= ~P8_KEY_X;
                else if (event.jbutton.button == 1 || event.jbutton.button == 3) s_joyHeldMask &= ~P8_KEY_O;
                break;

            case SDL_JOYHATMOTION:
                g_androidShowTouchControls = false;
                s_joyHeldMask &= ~(P8_KEY_LEFT | P8_KEY_RIGHT | P8_KEY_UP | P8_KEY_DOWN);
                if (event.jhat.value & SDL_HAT_LEFT)  s_joyHeldMask |= P8_KEY_LEFT;
                if (event.jhat.value & SDL_HAT_RIGHT) s_joyHeldMask |= P8_KEY_RIGHT;
                if (event.jhat.value & SDL_HAT_UP)    s_joyHeldMask |= P8_KEY_UP;
                if (event.jhat.value & SDL_HAT_DOWN)  s_joyHeldMask |= P8_KEY_DOWN;
                break;

            case SDL_JOYAXISMOTION:
                if (event.jaxis.axis == 0) {
                    if (event.jaxis.value < -20000) {
                        g_androidShowTouchControls = false;
                        s_joyHeldMask |= P8_KEY_LEFT;
                        s_joyHeldMask &= ~P8_KEY_RIGHT;
                    } else if (event.jaxis.value > 20000) {
                        g_androidShowTouchControls = false;
                        s_joyHeldMask |= P8_KEY_RIGHT;
                        s_joyHeldMask &= ~P8_KEY_LEFT;
                    } else if (abs(event.jaxis.value) < 14000) {
                        s_joyHeldMask &= ~(P8_KEY_LEFT | P8_KEY_RIGHT);
                    }
                } else if (event.jaxis.axis == 1) {
                    if (event.jaxis.value < -20000) {
                        g_androidShowTouchControls = false;
                        s_joyHeldMask |= P8_KEY_UP;
                        s_joyHeldMask &= ~P8_KEY_DOWN;
                    } else if (event.jaxis.value > 20000) {
                        g_androidShowTouchControls = false;
                        s_joyHeldMask |= P8_KEY_DOWN;
                        s_joyHeldMask &= ~P8_KEY_UP;
                    } else if (abs(event.jaxis.value) < 14000) {
                        s_joyHeldMask &= ~(P8_KEY_UP | P8_KEY_DOWN);
                    }
                }
                break;
        }
    }

    // Direct multi-touch evaluation from SDL2's live active finger table (zero ghost touches!)
    uint8_t touchMask = 0;
    bool menuTapped = false;
    bool importTapped = false;
    bool scaleTapped = false;
    bool stickTouchFound = false;

    if (g_androidShowTouchControls) {
        bool portrait = (_windowHeight >= _windowWidth);
        int numTouchDevs = SDL_GetNumTouchDevices();
        for (int d = 0; d < numTouchDevs; d++) {
            SDL_TouchID tid = SDL_GetTouchDevice(d);
            int numFingers = SDL_GetNumTouchFingers(tid);
            for (int f = 0; f < numFingers; f++) {
                SDL_Finger* finger = SDL_GetTouchFinger(tid, f);
                if (!finger) continue;
                int tx = (int)(finger->x * _windowWidth);
                int ty = (int)(finger->y * _windowHeight);

                // Top menu pills (generous 28px padding for easy fingertip access)
                if (pointInExpandedRect(tx, ty, g_btnMenuRect, 28)) {
                    menuTapped = true;
                } else if (pointInExpandedRect(tx, ty, g_btnImportRect, 28)) {
                    importTapped = true;
                } else if (pointInExpandedRect(tx, ty, g_btnScaleRect, 28)) {
                    scaleTapped = true;
                }

                // Transparent Thumb Virtual Joystick (Left Control Zone)
                bool inLeftZone = portrait ? (tx < _windowWidth * 55 / 100 && ty > DestR.y + DestR.h)
                                           : (tx < DestR.x && ty > 60);
                if (inLeftZone) {
                    float dx = (float)(tx - g_stickBaseX);
                    float dy = (float)(ty - g_stickBaseY);
                    float dist = sqrtf(dx * dx + dy * dy);
                    if (dist <= g_stickRadius * 2.5f) {
                        stickTouchFound = true;
                        g_stickActive = true;
                        float maxNubDist = (float)g_stickRadius;
                        if (dist > maxNubDist) {
                            g_stickNubX = g_stickBaseX + (int)(dx / dist * maxNubDist);
                            g_stickNubY = g_stickBaseY + (int)(dy / dist * maxNubDist);
                        } else {
                            g_stickNubX = tx;
                            g_stickNubY = ty;
                        }

                        float deadzone = g_stickRadius * 0.22f;
                        if (dist > deadzone) {
                            float thresh = deadzone * 0.70f;
                            if (dx < -thresh) touchMask |= P8_KEY_LEFT;
                            if (dx >  thresh) touchMask |= P8_KEY_RIGHT;
                            if (dy < -thresh) touchMask |= P8_KEY_UP;
                            if (dy >  thresh) touchMask |= P8_KEY_DOWN;
                        }
                    }
                }

                // Circular Action Buttons O (Red) and X (Blue) with 24px pad
                if (pointInCircle(tx, ty, g_btnOCenterX, g_btnOCenterY, g_btnORadius, 24)) {
                    touchMask |= P8_KEY_O;
                }
                if (pointInCircle(tx, ty, g_btnXCenterX, g_btnXCenterY, g_btnXRadius, 24)) {
                    touchMask |= P8_KEY_X;
                }

                // Virtual mouse inside PICO-8 screen
                if (DestR.w > 0 && DestR.h > 0 &&
                    tx >= DestR.x && tx < DestR.x + DestR.w &&
                    ty >= DestR.y && ty < DestR.y + DestR.h) {
                    float sx = DestR.w / 128.0f;
                    float sy = DestR.h / 128.0f;
                    mouseX = (int)((tx - DestR.x) / sx);
                    mouseY = (int)((ty - DestR.y) / sy);
                    mouseBtnState = 1;
                }
            }
        }

        if (!stickTouchFound) {
            g_stickActive = false;
            g_stickNubX = g_stickBaseX;
            g_stickNubY = g_stickBaseY;
        }
    } else {
        g_stickActive = false;
    }

    // Top button edge detection (fires only once per tap)
    if (menuTapped && !s_prevMenuTap) {
        kDown |= P8_KEY_PAUSE;
    }
    s_prevMenuTap = menuTapped;

    if (importTapped && !s_prevImportTap) {
        triggerAndroidImportPicker();
    }
    s_prevImportTap = importTapped;

    if (scaleTapped && !s_prevScaleTap) {
        stretchKeyPressed = true;
    }
    s_prevScaleTap = scaleTapped;

    g_androidTouchHeldMask = touchMask;
    uint8_t combinedHeld = g_androidShowTouchControls ? g_androidTouchHeldMask : (currKHeld | s_joyHeldMask);
    kDown |= (combinedHeld & ~s_prevCombinedHeld);
    s_prevCombinedHeld = combinedHeld;

    changeStretch();

    return InputState_t {
        kDown,
        combinedHeld,
        (int16_t)mouseX,
        (int16_t)mouseY,
        mouseBtnState,
        false,
        ""
    };
}

std::vector<std::string> Host::listcarts() {
    std::vector<std::string> carts;
    DIR *dir = opendir(_cartDirectory.c_str());
    if (dir != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (isCartFile(ent->d_name)) {
                carts.push_back(ent->d_name);
            }
        }
        closedir(dir);
    }
    std::sort(carts.begin(), carts.end());
    return carts;
}

std::vector<std::string> Host::listdirs() {
    return std::vector<std::string>();
}
