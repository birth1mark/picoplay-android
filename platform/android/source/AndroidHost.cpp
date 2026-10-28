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

extern bool g_androidShowTouchControls;
extern uint8_t g_androidTouchHeldMask;
extern SDL_Rect g_dpadRect;
extern SDL_Rect g_btnORect;
extern SDL_Rect g_btnXRect;
extern SDL_Rect g_btnMenuRect;
extern SDL_Rect g_btnImportRect;
extern SDL_Rect g_btnScaleRect;

struct TouchPoint {
    SDL_FingerID id;
    int x;
    int y;
    bool active;
};

static TouchPoint s_touches[10] = {};
static uint8_t s_prevCombinedHeld = 0;
static uint8_t s_joyHeldMask = 0;

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
    return (px >= r.x - pad && px <= r.x + r.w + pad &&
            py >= r.y - pad && py <= r.y + r.h + pad);
}

static uint8_t evaluateTouchButtons() {
    if (!g_androidShowTouchControls) return 0;
    uint8_t mask = 0;

    int dpadCx = g_dpadRect.x + g_dpadRect.w / 2;
    int dpadCy = g_dpadRect.y + g_dpadRect.h / 2;
    int deadzone = g_dpadRect.w / 9;

    for (int i = 0; i < 10; i++) {
        if (!s_touches[i].active) continue;
        int tx = s_touches[i].x;
        int ty = s_touches[i].y;

        if (pointInExpandedRect(tx, ty, g_dpadRect, 24)) {
            int dx = tx - dpadCx;
            int dy = ty - dpadCy;
            if (dx < -deadzone) mask |= P8_KEY_LEFT;
            if (dx >  deadzone) mask |= P8_KEY_RIGHT;
            if (dy < -deadzone) mask |= P8_KEY_UP;
            if (dy >  deadzone) mask |= P8_KEY_DOWN;
        }
        if (pointInExpandedRect(tx, ty, g_btnORect, 18)) {
            mask |= P8_KEY_O;
        }
        if (pointInExpandedRect(tx, ty, g_btnXRect, 18)) {
            mask |= P8_KEY_X;
        }
    }
    return mask;
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

    if (windowWidth == 0) windowWidth = 1080;
    if (windowHeight == 0) windowHeight = 2400;

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
}

InputState_t Host::scanInput() {
    uint8_t kDown = 0;
    uint8_t mouseBtnState = 0;
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

            case SDL_FINGERDOWN: {
                g_androidShowTouchControls = true;
                int fx = (int)(event.tfinger.x * _windowWidth);
                int fy = (int)(event.tfinger.y * _windowHeight);

                if (pointInExpandedRect(fx, fy, g_btnMenuRect, 8)) {
                    kDown |= P8_KEY_PAUSE;
                    break;
                }
                if (pointInExpandedRect(fx, fy, g_btnImportRect, 8)) {
                    triggerAndroidImportPicker();
                    break;
                }
                if (pointInExpandedRect(fx, fy, g_btnScaleRect, 8)) {
                    stretchKeyPressed = true;
                    break;
                }

                for (int i = 0; i < 10; i++) {
                    if (!s_touches[i].active) {
                        s_touches[i].id = event.tfinger.fingerId;
                        s_touches[i].x = fx;
                        s_touches[i].y = fy;
                        s_touches[i].active = true;
                        break;
                    }
                }
                break;
            }

            case SDL_FINGERMOTION: {
                int fx = (int)(event.tfinger.x * _windowWidth);
                int fy = (int)(event.tfinger.y * _windowHeight);
                for (int i = 0; i < 10; i++) {
                    if (s_touches[i].active && s_touches[i].id == event.tfinger.fingerId) {
                        s_touches[i].x = fx;
                        s_touches[i].y = fy;
                        break;
                    }
                }
                break;
            }

            case SDL_FINGERUP: {
                for (int i = 0; i < 10; i++) {
                    if (s_touches[i].active && s_touches[i].id == event.tfinger.fingerId) {
                        s_touches[i].active = false;
                        break;
                    }
                }
                break;
            }

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_AC_BACK || event.key.keysym.sym == SDLK_ESCAPE || event.key.keysym.sym == SDLK_RETURN) {
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
                    s_joyHeldMask &= ~(P8_KEY_LEFT | P8_KEY_RIGHT);
                    if (event.jaxis.value < -12000) { g_androidShowTouchControls = false; s_joyHeldMask |= P8_KEY_LEFT; }
                    else if (event.jaxis.value > 12000) { g_androidShowTouchControls = false; s_joyHeldMask |= P8_KEY_RIGHT; }
                } else if (event.jaxis.axis == 1) {
                    s_joyHeldMask &= ~(P8_KEY_UP | P8_KEY_DOWN);
                    if (event.jaxis.value < -12000) { g_androidShowTouchControls = false; s_joyHeldMask |= P8_KEY_UP; }
                    else if (event.jaxis.value > 12000) { g_androidShowTouchControls = false; s_joyHeldMask |= P8_KEY_DOWN; }
                }
                break;
        }
    }

    g_androidTouchHeldMask = evaluateTouchButtons();
    uint8_t combinedHeld = currKHeld | s_joyHeldMask | g_androidTouchHeldMask;
    kDown |= (combinedHeld & ~s_prevCombinedHeld);
    s_prevCombinedHeld = combinedHeld;

    changeStretch();

    float effScaleX = (DestR.w > 0) ? (DestR.w / 128.0f) : 1.0f;
    float effScaleY = (DestR.h > 0) ? (DestR.h / 128.0f) : 1.0f;
    int mouseX = 0, mouseY = 0;
    for (int i = 0; i < 10; i++) {
        if (s_touches[i].active &&
            s_touches[i].x >= DestR.x && s_touches[i].x < DestR.x + DestR.w &&
            s_touches[i].y >= DestR.y && s_touches[i].y < DestR.y + DestR.h) {
            mouseX = (int)((s_touches[i].x - DestR.x) / effScaleX);
            mouseY = (int)((s_touches[i].y - DestR.y) / effScaleY);
            mouseBtnState = 1;
            break;
        }
    }

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
