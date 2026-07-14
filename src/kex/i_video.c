// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// Copyright(C) 2005 Simon Howard
// Copyright(C) 2007-2012 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//    SDL Stuff
//
//-----------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdbool.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "m_misc.h"
#include "doomdef.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "g_actions.h"

#ifdef DOOM64EX_UWP
#include "uwp_video.h"
#endif
#include "d_main.h"
#include "gl_main.h"

extern SDL_Window* window;
bool	window_mouse;

CVAR(v_msensitivityx, 5);
CVAR(v_msensitivityy, 5);
CVAR(v_gamepadsensitivityx, 5);
CVAR(v_gamepadsensitivityy, 5);
CVAR(v_gamepadlook, 1);
CVAR(v_gamepadlookinvert, 0);
CVAR(v_macceleration, 0);
CVAR(v_mlook, 0);
CVAR(v_mlookinvert, 0);
CVAR(v_yaxismove, 0);
CVAR(v_width, 640);
CVAR(v_height, 480);
CVAR(v_windowed, 1);
CVAR(v_vsync, 1);
CVAR(v_depthsize, 24);
CVAR(v_buffersize, 32);

CVAR_EXTERNAL(m_menumouse);

static void I_GetEvent(SDL_Event *Event);
static void I_ReadMouse(void);
static void I_InitInputs(void);
dboolean I_UpdateGrab(void);

//================================================================================
// Video
//================================================================================

SDL_Surface *screen;
int    video_width;
int    video_height;
float video_ratio;
dboolean window_focused;

float mouse_x = 0.0f;
float mouse_y = 0.0f;

//
// I_InitScreen
//

void I_InitScreen(void) {
    int        newwidth;
    int        newheight;
    int        p;

    InWindow = (int)v_windowed.value;
    video_width = (int)v_width.value;
    video_height = (int)v_height.value;
    video_ratio = (float)video_width / (float)video_height;

    if(M_CheckParm("-window")) {
        InWindow=true;
    }
    if(M_CheckParm("-fullscreen")) {
        InWindow=false;
    }

    newwidth = newheight = 0;

    p = M_CheckParm("-width");
    if(p && p < myargc - 1) {
        newwidth = datoi(myargv[p+1]);
    }

    p = M_CheckParm("-height");
    if(p && p < myargc - 1) {
        newheight = datoi(myargv[p+1]);
    }

    if(newwidth && newheight) {
        video_width = newwidth;
        video_height = newheight;
        CON_CvarSetValue(v_width.name, (float)video_width);
        CON_CvarSetValue(v_height.name, (float)video_height);
    }

    if(v_depthsize.value != 8 &&
            v_depthsize.value != 16 &&
            v_depthsize.value != 24) {
        CON_CvarSetValue(v_depthsize.name, 24);
    }

    if(v_buffersize.value != 8 &&
            v_buffersize.value != 16 &&
            v_buffersize.value != 24
            && v_buffersize.value != 32) {
        CON_CvarSetValue(v_buffersize.name, 32);
    }

    usingGL = false;
}

//
// I_ShutdownWait
//

int I_ShutdownWait(void) {
    static SDL_Event event;

    while(SDL_PollEvent(&event)) {
        if(event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
            I_ShutdownVideo();
#ifndef USESYSCONSOLE
            exit(0);
#else
            return 1;
#endif
        }
    }

    return 0;
}

//
// I_ShutdownVideo
//

void I_ShutdownVideo(void) {
    SDL_Quit();
}

//
// I_NetWaitScreen
// Blank screen display while waiting for players to join
//

void I_NetWaitScreen(void) {

}

//
// I_InitVideo
//

void I_InitVideo(void) {
    char title[256];

    Uint32 f = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER;

    if(SDL_Init(f) < 0) {
        printf("ERROR - Failed to initialize SDL");
        exit(1);
    }

    sprintf(title, "DOOM64EX-Classic - Version Date: %s", version_date);

    I_InitInputs();
}

//
// I_StartTic
//

void I_StartTic(void) {
    SDL_Event Event;

    while(SDL_PollEvent(&Event)) {
        I_GetEvent(&Event);
    }

    I_GamepadUpdate();

    I_InitInputs();
    I_ReadMouse();
}

//
// I_FinishUpdate
//

void I_FinishUpdate(void) {
    I_UpdateGrab();
    SDL_GL_SwapWindow(window);

#ifdef DOOM64EX_UWP
    doom64ex_uwp_after_present(window);
#endif

    BusyDisk = false;
}

//================================================================================
// Input
//================================================================================

float mouse_accelfactor;

int            UseJoystick;
int            UseMouse[2];
dboolean    DigiJoy;
int            DualMouse;

dboolean    MouseMode;//false=microsoft, true=mouse systems

//
// SDL controller input.
//

#define GAMEPAD_MENU_INITIAL_DELAY_TICS  12
#define GAMEPAD_MENU_REPEAT_TICS         4
#define GAMEPAD_MENU_STICK_THRESH        0.50f
#define GAMEPAD_TRIGGER_THRESHOLD 0.30f
#define GAMEPAD_INNER_DZ_LEFT    0.18f
#define GAMEPAD_INNER_DZ_RIGHT   0.20f
#define GAMEPAD_OUTER_DZ         0.02f
#define GAMEPAD_ANTI_DZ          0.04f
#define GAMEPAD_EXPO_LEFT        1.20f
#define GAMEPAD_EXPO_RIGHT       1.60f

extern void D_PostEvent(event_t*);
extern gamestate_t gamestate;

static struct {
    SDL_GameController* gamepad;
    SDL_JoystickID active_id;

    bool action[NUM_CONTROLLER_BUTTONS];
    bool in_menu;

    bool mouse_up, mouse_down, mouse_left, mouse_right;
    bool mouse_accept, mouse_back, mouse_scroll_up, mouse_scroll_down, mouse_delete;
    int menu_up_tic, menu_down_tic, menu_left_tic, menu_right_tic;
    int menu_accept_tic, menu_back_tic, menu_page_up_tic, menu_page_down_tic, menu_delete_tic;

    bool init;
} gamepad64;

static const SDL_GameControllerButton ControllerButtonMap[CONTROLLER_LEFT_TRIGGER] = {
    SDL_CONTROLLER_BUTTON_A,
    SDL_CONTROLLER_BUTTON_B,
    SDL_CONTROLLER_BUTTON_X,
    SDL_CONTROLLER_BUTTON_Y,
    SDL_CONTROLLER_BUTTON_BACK,
    SDL_CONTROLLER_BUTTON_START,
    SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
    SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
    SDL_CONTROLLER_BUTTON_LEFTSTICK,
    SDL_CONTROLLER_BUTTON_RIGHTSTICK,
    SDL_CONTROLLER_BUTTON_DPAD_UP,
    SDL_CONTROLLER_BUTTON_DPAD_DOWN,
    SDL_CONTROLLER_BUTTON_DPAD_LEFT,
    SDL_CONTROLLER_BUTTON_DPAD_RIGHT
};

// Menu input gets its own events, so controller navigation never fakes keyboard input.

static SDL_INLINE float I_GamepadClamp(float x) { return SDL_clamp(x, 0.f, 1.f); }

static void I_GamepadRadialLookSmoothing(float x, float y,
    float inner_dz, float outer_dz,
    float expo, float anti_dz,
    float* ox, float* oy)
{
    float r = SDL_sqrtf(x * x + y * y);
    if (r <= inner_dz) { *ox = 0.f; *oy = 0.f; return; }

    float nx = x / (r > 0.f ? r : 1.f);
    float ny = y / (r > 0.f ? r : 1.f);
    float span = 1.f - inner_dz - outer_dz;
    float t = I_GamepadClamp((r - inner_dz) / (span > 0.f ? span : 1.f));

    t = SDL_powf(t, expo);
    t = anti_dz + (1.f - anti_dz) * t;

    *ox = nx * t;
    *oy = ny * t;
}

static SDL_INLINE void I_GamepadPostMenuEvent(gamepad_menu_event_t action) {
    event_t ev;

    ev.type = ev_gamepad;
    ev.data1 = action;
    ev.data2 = ev.data3 = 0;
    ev.data4 = 0;
    D_PostEvent(&ev);
}

static SDL_INLINE void I_GamepadMenuEvent(bool now, bool* prev, gamepad_menu_event_t action, int* next_tic) {
    if (now && !*prev) {
        I_GamepadPostMenuEvent(action);
        *next_tic = gametic + GAMEPAD_MENU_INITIAL_DELAY_TICS;
    }
    else if (now && *prev && gametic >= *next_tic) {
        I_GamepadPostMenuEvent(action);
        *next_tic = gametic + GAMEPAD_MENU_REPEAT_TICS;
    }
    *prev = now;
}

static SDL_INLINE void I_GamepadButtonEvent(bool now, bool* prev, controller_button_t button) {
    event_t ev;

    if(now == *prev) {
        return;
    }

    ev.type = now ? ev_gamepaddown : ev_gamepadup;
    ev.data1 = button;
    ev.data2 = ev.data3 = 0;
    ev.data4 = 0;
    D_PostEvent(&ev);
    *prev = now;
}

static void I_GamepadAxisEvent(controller_axis_t positive, controller_axis_t negative, float value) {
    event_t ev;

    if(value == 0.0f) {
        return;
    }

    ev.type = ev_gamepadaxis;
    ev.data1 = value > 0.0f ? positive : negative;
    ev.data2 = fabsf(value);
    ev.data3 = 0.0f;
    ev.data4 = 0;
    D_PostEvent(&ev);
}

static void I_GamepadStickAxisEvents(controller_axis_t right, controller_axis_t left,
        controller_axis_t down, controller_axis_t up, float x, float y) {
    // Send the dominant direction first so capture picks the direction the player meant.
    if(fabsf(y) > fabsf(x)) {
        I_GamepadAxisEvent(down, up, y);
        I_GamepadAxisEvent(right, left, x);
    }
    else {
        I_GamepadAxisEvent(right, left, x);
        I_GamepadAxisEvent(down, up, y);
    }
}

static SDL_INLINE float I_GamepadAxisAlive(Sint16 v) {
    const float maxmag = (float)SDL_JOYSTICK_AXIS_MAX;
    float f = (float)v / maxmag;
    f = SDL_clamp(f, -1.f, 1.f);
    return f;
}
static void I_GamepadInit(void) {
    int index;
    const int count = SDL_NumJoysticks();
    for (index = 0; index < count && !gamepad64.gamepad; ++index) {
        if (SDL_IsGameController(index)) {
            gamepad64.gamepad = SDL_GameControllerOpen(index);
            if (gamepad64.gamepad) gamepad64.active_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad64.gamepad));
        }
    }
}
static void I_GamepadClose(void) {
    if (gamepad64.gamepad) { SDL_GameControllerClose(gamepad64.gamepad); gamepad64.gamepad = NULL; }
    gamepad64.active_id = 0;
}

static void I_GamepadInitOnce(void) {
    if (gamepad64.init) return;
    if (!(SDL_WasInit(SDL_INIT_GAMECONTROLLER) & SDL_INIT_GAMECONTROLLER)) SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    I_GamepadInit();
    gamepad64.init = true;
}

static void I_GamepadHandleSDLEvent(const SDL_Event* e) {
    if (!gamepad64.init) return;
    switch (e->type) {
    case SDL_CONTROLLERDEVICEADDED:
        if (!gamepad64.gamepad) {
            gamepad64.gamepad = SDL_GameControllerOpen(e->cdevice.which);
            if (gamepad64.gamepad) gamepad64.active_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad64.gamepad));
        }
        break;
    case SDL_CONTROLLERDEVICEREMOVED:
        if (gamepad64.active_id == e->cdevice.which) I_GamepadClose();
        break;
    case SDL_JOYDEVICEADDED:
        if (!gamepad64.gamepad) {
            if (SDL_IsGameController(e->jdevice.which)) {
                gamepad64.gamepad = SDL_GameControllerOpen(e->jdevice.which);
                if (gamepad64.gamepad) gamepad64.active_id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gamepad64.gamepad));
            }
        }
        break;
    case SDL_JOYDEVICEREMOVED:
        if (gamepad64.active_id == e->jdevice.which) I_GamepadClose();
        break;
    default: break;
    }
}

void I_GamepadUpdate(void) {
    if (!gamepad64.init) return;

    SDL_GameControllerUpdate();

    float lx_raw = 0.f, ly_raw = 0.f, rx_raw = 0.f, ry_raw = 0.f;
    float lt = 0.f, rt = 0.f;

    if (gamepad64.gamepad) {
        lx_raw = I_GamepadAxisAlive(SDL_GameControllerGetAxis(gamepad64.gamepad, SDL_CONTROLLER_AXIS_LEFTX));
        ly_raw = I_GamepadAxisAlive(SDL_GameControllerGetAxis(gamepad64.gamepad, SDL_CONTROLLER_AXIS_LEFTY));
        rx_raw = I_GamepadAxisAlive(SDL_GameControllerGetAxis(gamepad64.gamepad, SDL_CONTROLLER_AXIS_RIGHTX));
        ry_raw = I_GamepadAxisAlive(SDL_GameControllerGetAxis(gamepad64.gamepad, SDL_CONTROLLER_AXIS_RIGHTY));
        lt = (float)SDL_GameControllerGetAxis(gamepad64.gamepad, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / (float)SDL_JOYSTICK_AXIS_MAX;
        rt = (float)SDL_GameControllerGetAxis(gamepad64.gamepad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / (float)SDL_JOYSTICK_AXIS_MAX;
    }
    float lx = 0.f, ly = 0.f, rx = 0.f, ry = 0.f;
    I_GamepadRadialLookSmoothing(lx_raw, ly_raw, GAMEPAD_INNER_DZ_LEFT, GAMEPAD_OUTER_DZ, GAMEPAD_EXPO_LEFT, GAMEPAD_ANTI_DZ, &lx, &ly);
    I_GamepadRadialLookSmoothing(rx_raw, ry_raw, GAMEPAD_INNER_DZ_RIGHT, GAMEPAD_OUTER_DZ, GAMEPAD_EXPO_RIGHT, GAMEPAD_ANTI_DZ, &rx, &ry);

    I_GamepadStickAxisEvents(CONTROLLER_AXIS_LEFT_RIGHT, CONTROLLER_AXIS_LEFT_LEFT,
        CONTROLLER_AXIS_LEFT_DOWN, CONTROLLER_AXIS_LEFT_UP, lx, ly);
    I_GamepadStickAxisEvents(CONTROLLER_AXIS_RIGHT_RIGHT, CONTROLLER_AXIS_RIGHT_LEFT,
        CONTROLLER_AXIS_RIGHT_DOWN, CONTROLLER_AXIS_RIGHT_UP, rx, ry);

    // Intermissions and finales still need the player's normal bindings, such as +use.
    const bool in_menu = menuactive;

    if(in_menu != gamepad64.in_menu) {
        if(in_menu) {
            G_ReleaseControllerActions();
        }
        gamepad64.in_menu = in_menu;
    }

    {
        int i;

        for(i = 0; i < CONTROLLER_LEFT_TRIGGER; i++) {
            const bool pressed = gamepad64.gamepad && SDL_GameControllerGetButton(gamepad64.gamepad, ControllerButtonMap[i]) != 0;
            if(i != CONTROLLER_MENU) {
                I_GamepadButtonEvent(pressed, &gamepad64.action[i], (controller_button_t)i);
            }
        }
        I_GamepadButtonEvent(lt >= GAMEPAD_TRIGGER_THRESHOLD, &gamepad64.action[CONTROLLER_LEFT_TRIGGER], CONTROLLER_LEFT_TRIGGER);
        I_GamepadButtonEvent(rt >= GAMEPAD_TRIGGER_THRESHOLD, &gamepad64.action[CONTROLLER_RIGHT_TRIGGER], CONTROLLER_RIGHT_TRIGGER);
    }

    if (in_menu) {
        bool d_up = false, d_down = false, d_left = false, d_right = false;
        if (gamepad64.gamepad) {
            d_up = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            d_down = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            d_left = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;
            d_right = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
        }
        bool s_up = false, s_down = false, s_left = false, s_right = false;
        if (!(d_up | d_down | d_left | d_right)) {
            s_up = (-ly) > GAMEPAD_MENU_STICK_THRESH;
            s_down = (-ly) < -GAMEPAD_MENU_STICK_THRESH;
            s_left = (lx) < -GAMEPAD_MENU_STICK_THRESH;
            s_right = (lx) > GAMEPAD_MENU_STICK_THRESH;
        }
        const bool mouse_up = d_up || s_up;
        const bool mouse_down = d_down || s_down;
        const bool mouse_left = d_left || s_left;
        const bool mouse_right = d_right || s_right;

        I_GamepadMenuEvent(mouse_up, &gamepad64.mouse_up, GAMEPAD_MENU_UP, &gamepad64.menu_up_tic);
        I_GamepadMenuEvent(mouse_down, &gamepad64.mouse_down, GAMEPAD_MENU_DOWN, &gamepad64.menu_down_tic);
        I_GamepadMenuEvent(mouse_left, &gamepad64.mouse_left, GAMEPAD_MENU_LEFT, &gamepad64.menu_left_tic);
        I_GamepadMenuEvent(mouse_right, &gamepad64.mouse_right, GAMEPAD_MENU_RIGHT, &gamepad64.menu_right_tic);

        bool a = false, b = false, x = false, startbtn = false, lb = false, rb = false;
        if (gamepad64.gamepad) {
            a = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_A) != 0;
            b = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_B) != 0;
            x = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_X) != 0;
            startbtn = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_START) != 0;
            lb = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
            rb = SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
        }
        I_GamepadMenuEvent(a, &gamepad64.mouse_accept, GAMEPAD_MENU_ACCEPT, &gamepad64.menu_accept_tic);
        I_GamepadMenuEvent((b | startbtn), &gamepad64.mouse_back, GAMEPAD_MENU_BACK, &gamepad64.menu_back_tic);
        I_GamepadMenuEvent(lb, &gamepad64.mouse_scroll_up, GAMEPAD_MENU_PAGE_UP, &gamepad64.menu_page_up_tic);
        I_GamepadMenuEvent(rb, &gamepad64.mouse_scroll_down, GAMEPAD_MENU_PAGE_DOWN, &gamepad64.menu_page_down_tic);
        I_GamepadMenuEvent(x, &gamepad64.mouse_delete, GAMEPAD_MENU_DELETE, &gamepad64.menu_delete_tic);
        return;
    }
    I_GamepadMenuEvent(gamepad64.gamepad && SDL_GameControllerGetButton(gamepad64.gamepad, SDL_CONTROLLER_BUTTON_START) != 0,
        &gamepad64.mouse_back, GAMEPAD_MENU_BACK, &gamepad64.menu_back_tic);
}

//
// I_TranslateKey
//

// Modernised, it was really needed!
static int I_TranslateKey(const int key)
{
    struct { int sdl; int eng; } map[] = {
        { SDLK_LEFT,        KEY_LEFTARROW },
        { SDLK_RIGHT,       KEY_RIGHTARROW },
        { SDLK_UP,          KEY_UPARROW },
        { SDLK_DOWN,        KEY_DOWNARROW },
        { SDLK_ESCAPE,      KEY_ESCAPE },
        { SDLK_RETURN,      KEY_ENTER },
        { SDLK_TAB,         KEY_TAB },
        { SDLK_BACKSPACE,   KEY_BACKSPACE },
        { SDLK_DELETE,      KEY_DEL },
        { SDLK_INSERT,      KEY_INSERT },
        { SDLK_HOME,        KEY_HOME },
        { SDLK_END,         KEY_END },
        { SDLK_PAGEUP,      KEY_PAGEUP },
        { SDLK_PAGEDOWN,    KEY_PAGEDOWN },
        { SDLK_PAUSE,       KEY_PAUSE },
        { SDLK_LSHIFT,      KEY_RSHIFT },
        { SDLK_RSHIFT,      KEY_RSHIFT },
        { SDLK_LCTRL,       KEY_RCTRL },
        { SDLK_RCTRL,       KEY_RCTRL },
        { SDLK_LALT,        KEY_RALT },
        { SDLK_RALT,        KEY_RALT },
        { SDLK_EQUALS,      KEY_EQUALS },
        { SDLK_MINUS,       KEY_MINUS },
        { SDLK_SPACE,       KEY_SPACEBAR },
        { SDLK_F1,          KEY_F1 },
        { SDLK_F2,			KEY_F2  },
        { SDLK_F3,			KEY_F3  },
        { SDLK_F4,			KEY_F4  },
        { SDLK_F5,          KEY_F5 },
        { SDLK_F6,			KEY_F6  },
        { SDLK_F7,			KEY_F7  },
        { SDLK_F8,			KEY_F8  },
        { SDLK_F9,          KEY_F9 },
        { SDLK_F10,			KEY_F10 },
        { SDLK_F11,			KEY_F11 },
        { SDLK_F12,			KEY_F12 },
        { SDLK_KP_0,        KEY_KEYPAD0 },
        { SDLK_KP_1,        KEY_KEYPAD1 },
        { SDLK_KP_2,        KEY_KEYPAD2 },
        { SDLK_KP_3,        KEY_KEYPAD3 },
        { SDLK_KP_4,        KEY_KEYPAD4 },
        { SDLK_KP_5,        KEY_KEYPAD5 },
        { SDLK_KP_6,        KEY_KEYPAD6 },
        { SDLK_KP_7,        KEY_KEYPAD7 },
        { SDLK_KP_8,        KEY_KEYPAD8 },
        { SDLK_KP_9,        KEY_KEYPAD9 },
        { SDLK_KP_ENTER,    KEY_KEYPADENTER },
        { SDLK_KP_MULTIPLY, KEY_KEYPADMULTIPLY },
        { SDLK_KP_PLUS,     KEY_KEYPADPLUS },
        { SDLK_KP_MINUS,    KEY_KEYPADMINUS },
        { SDLK_KP_DIVIDE,   KEY_KEYPADDIVIDE },
        { SDLK_KP_PERIOD,   KEY_KEYPADPERIOD },
    };
    for (size_t i = 0; i < sizeof(map) / sizeof(map[0]); ++i) {
        if (key == map[i].sdl) return map[i].eng;
    }
    if (key >= 32 && key < 127) {
        return key;
    }
    return 0;
}

//
// I_SDLtoDoomMouseState
//

static int I_SDLtoDoomMouseState(Uint32 buttonstate) {
    return 0
        | (buttonstate & SDL_BUTTON_LMASK ? 1 : 0)
        | (buttonstate & SDL_BUTTON_MMASK ? 2 : 0)
        | (buttonstate & SDL_BUTTON_RMASK ? 4 : 0)
        | (buttonstate & SDL_BUTTON_X1MASK ? 8 : 0)
        | (buttonstate & SDL_BUTTON_X2MASK ? 16 : 0);
}

//
// I_UpdateFocus
//

static void I_UpdateFocus(void) {

}

// I_CenterMouse
// Warp the mouse back to the middle of the screen
//

void I_CenterMouse(void) {
    SDL_WarpMouseInWindow(window, (unsigned short)(video_width / 2), (unsigned short)(video_height / 2));
    SDL_PumpEvents();
    SDL_GetRelativeMouseState(NULL, NULL);
}

//
// I_ReadMouse
//

static void I_ReadMouse(void) {
    int x, y;
    Uint32 btn;
    event_t ev;
    static Uint8 lastmbtn = 0;

    SDL_GetRelativeMouseState(&x, &y);
    btn = SDL_GetMouseState(NULL, NULL);

    if(x != 0 || y != 0 || btn || (lastmbtn != btn)) {
        ev.type = ev_mouse;
        ev.data1 = I_SDLtoDoomMouseState(btn);
        ev.data2 = (float)x * 32.0f;
        ev.data3 = (float)-y * 32.0f;
        ev.data4 = 0;
        D_PostEvent(&ev);
    }

    lastmbtn = btn;
}

//
// I_MouseAccelChange
//

void I_MouseAccelChange(void) {
    mouse_accelfactor = v_macceleration.value / 200.0f + 1.0f;
}

//
// I_MouseAccel
//

float I_MouseAccel(float val) {
    if (!v_macceleration.value) {
        return val;
    }

    if (val < 0) {
        return -I_MouseAccel(-val);
    }

    return (float)(pow((double)val, (double)mouse_accelfactor));
}

//
// I_UpdateGrab
//

dboolean I_UpdateGrab(void) {
    static dboolean currently_grabbed = 0;
    extern dboolean menuactive;
    dboolean grab = 1;

#ifndef DOOM64EX_UWP
    if (grab && !currently_grabbed) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
        SDL_SetWindowGrab(window, SDL_TRUE);
        currently_grabbed = true;
    }

    if (!grab && currently_grabbed) {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(window, SDL_FALSE);
        currently_grabbed = false;
    }
#endif

    currently_grabbed = grab;

    return currently_grabbed;
}

//
// I_GetEvent
//

static void I_GetEvent(SDL_Event* Event) {
    event_t event;
    unsigned int mwheeluptic = 0, mwheeldowntic = 0;
    unsigned int tic = gametic;

    I_GamepadHandleSDLEvent(Event);

    switch (Event->type) {
    case SDL_KEYDOWN:
        if (Event->key.repeat) {
            break;
        }
        event.type = ev_keydown;
        event.data1 = I_TranslateKey(Event->key.keysym.sym);
        D_PostEvent(&event);
        break;

    case SDL_KEYUP:
        event.type = ev_keyup;
        event.data1 = I_TranslateKey(Event->key.keysym.sym);
        D_PostEvent(&event);
        break;

    case SDL_TEXTINPUT:
        for(const unsigned char* text = (const unsigned char*)Event->text.text; *text; text++) {
            // The menu font is ASCII-only, so leave multibyte characters alone.
            if(*text < 0x80) {
                event.type = ev_textinput;
                event.data1 = *text;
                event.data2 = event.data3 = 0;
                event.data4 = 0;
                D_PostEvent(&event);
            }
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (!window_focused)
            break;

        event.type = (Event->type == SDL_MOUSEBUTTONUP) ? ev_mouseup : ev_mousedown;
        event.data1 =
            I_SDLtoDoomMouseState(SDL_GetMouseState(NULL, NULL));
        event.data2 = event.data3 = 0;

        D_PostEvent(&event);
        break;

    case SDL_MOUSEWHEEL:
        if (Event->wheel.y > 0) {
            event.type = ev_keydown;
            event.data1 = KEY_MWHEELUP;
            event.data2 = event.data3 = 0;
            D_PostEvent(&event);  
            event.type = ev_keyup;
            D_PostEvent(&event);
        }
        else if (Event->wheel.y < 0) {
            event.type = ev_keydown;
            event.data1 = KEY_MWHEELDOWN;
            event.data2 = event.data3 = 0;
            D_PostEvent(&event);
            event.type = ev_keyup;
            D_PostEvent(&event);
        }
        else
            break;
        break;

    case SDL_WINDOWEVENT:
        switch (Event->window.event) {
        case SDL_WINDOWEVENT_FOCUS_GAINED: window_focused = true; break;
        case SDL_WINDOWEVENT_FOCUS_LOST: window_focused = false; break;
        case SDL_WINDOWEVENT_ENTER: window_mouse = true; break;
        case SDL_WINDOWEVENT_LEAVE: window_mouse = false; break;
        default: break;
        }
        break;

    case SDL_QUIT:
        I_Quit();
        break;

    default:
        break;
    }

    if (mwheeluptic && mwheeluptic + 1 < tic) {
        event.type = ev_keyup;
        event.data1 = KEY_MWHEELUP;
        D_PostEvent(&event);
        mwheeluptic = 0;
    }

    if (mwheeldowntic && mwheeldowntic + 1 < tic) {
        event.type = ev_keyup;
        event.data1 = KEY_MWHEELDOWN;
        D_PostEvent(&event);
        mwheeldowntic = 0;
    }
}

//
// I_InitInputs
//

static void I_InitInputs(void) {
    SDL_PumpEvents();
    I_MouseAccelChange();
    I_GamepadInitOnce();
}
//
// V_RegisterCvars
//

void V_RegisterCvars(void) {
    CON_CvarRegister(&v_msensitivityx);
    CON_CvarRegister(&v_msensitivityy);
    CON_CvarRegister(&v_gamepadsensitivityx);
    CON_CvarRegister(&v_gamepadsensitivityy);
    CON_CvarRegister(&v_gamepadlook);
    CON_CvarRegister(&v_gamepadlookinvert);
    CON_CvarRegister(&v_macceleration);
    CON_CvarRegister(&v_mlook);
    CON_CvarRegister(&v_mlookinvert);
    CON_CvarRegister(&v_yaxismove);
    CON_CvarRegister(&v_width);
    CON_CvarRegister(&v_height);
    CON_CvarRegister(&v_windowed);
    CON_CvarRegister(&v_vsync);
    CON_CvarRegister(&v_depthsize);
    CON_CvarRegister(&v_buffersize);
}
