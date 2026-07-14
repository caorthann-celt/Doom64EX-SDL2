#include "uwp_wad_launcher.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <map>
#include <string>
#include <vector>

namespace {

struct wad_entry_t {
    std::wstring name;
    std::wstring path;
};

enum launcher_screen_t {
    launcher_main,
    launcher_wads
};

static const int kLogicalWidth = 640;
static const int kLogicalHeight = 480;
static const int kFontScale = 2;
static const int kGlyphWidth = 6 * kFontScale;
static const int kRowHeight = 24;
static const int kListRows = 11;

static const Uint8 kGlyphs[][7] = {
    { 0x0e,0x11,0x11,0x1f,0x11,0x11,0x11 }, // A
    { 0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e }, // B
    { 0x0e,0x11,0x10,0x10,0x10,0x11,0x0e }, // C
    { 0x1e,0x11,0x11,0x11,0x11,0x11,0x1e }, // D
    { 0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f }, // E
    { 0x1f,0x10,0x10,0x1e,0x10,0x10,0x10 }, // F
    { 0x0e,0x11,0x10,0x17,0x11,0x11,0x0e }, // G
    { 0x11,0x11,0x11,0x1f,0x11,0x11,0x11 }, // H
    { 0x0e,0x04,0x04,0x04,0x04,0x04,0x0e }, // I
    { 0x01,0x01,0x01,0x01,0x11,0x11,0x0e }, // J
    { 0x11,0x12,0x14,0x18,0x14,0x12,0x11 }, // K
    { 0x10,0x10,0x10,0x10,0x10,0x10,0x1f }, // L
    { 0x11,0x1b,0x15,0x15,0x11,0x11,0x11 }, // M
    { 0x11,0x19,0x15,0x13,0x11,0x11,0x11 }, // N
    { 0x0e,0x11,0x11,0x11,0x11,0x11,0x0e }, // O
    { 0x1e,0x11,0x11,0x1e,0x10,0x10,0x10 }, // P
    { 0x0e,0x11,0x11,0x11,0x15,0x12,0x0d }, // Q
    { 0x1e,0x11,0x11,0x1e,0x14,0x12,0x11 }, // R
    { 0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e }, // S
    { 0x1f,0x04,0x04,0x04,0x04,0x04,0x04 }, // T
    { 0x11,0x11,0x11,0x11,0x11,0x11,0x0e }, // U
    { 0x11,0x11,0x11,0x11,0x11,0x0a,0x04 }, // V
    { 0x11,0x11,0x11,0x15,0x15,0x15,0x0a }, // W
    { 0x11,0x11,0x0a,0x04,0x0a,0x11,0x11 }, // X
    { 0x11,0x11,0x0a,0x04,0x04,0x04,0x04 }, // Y
    { 0x1f,0x01,0x02,0x04,0x08,0x10,0x1f }, // Z
    { 0x0e,0x11,0x13,0x15,0x19,0x11,0x0e }, // 0
    { 0x04,0x0c,0x04,0x04,0x04,0x04,0x0e }, // 1
    { 0x0e,0x11,0x01,0x02,0x04,0x08,0x1f }, // 2
    { 0x1e,0x01,0x01,0x0e,0x01,0x01,0x1e }, // 3
    { 0x02,0x06,0x0a,0x12,0x1f,0x02,0x02 }, // 4
    { 0x1f,0x10,0x10,0x1e,0x01,0x01,0x1e }, // 5
    { 0x0e,0x10,0x10,0x1e,0x11,0x11,0x0e }, // 6
    { 0x1f,0x01,0x02,0x04,0x08,0x08,0x08 }, // 7
    { 0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e }, // 8
    { 0x0e,0x11,0x11,0x0f,0x01,0x01,0x0e }, // 9
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, // space
    { 0x00,0x00,0x00,0x00,0x00,0x0c,0x0c }, // .
    { 0x00,0x00,0x00,0x1f,0x00,0x00,0x00 }, // -
    { 0x00,0x00,0x00,0x00,0x00,0x00,0x1f }, // _
    { 0x01,0x02,0x04,0x08,0x10,0x00,0x00 }, // /
    { 0x00,0x04,0x04,0x1f,0x04,0x04,0x00 }, // +
    { 0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00 }, // :
    { 0x0e,0x11,0x01,0x02,0x04,0x00,0x04 }, // ?
    { 0x10,0x08,0x04,0x02,0x04,0x08,0x10 }  // >
};

static std::wstring Lowercase(const std::wstring &value)
{
    std::wstring result = value;
    std::transform(result.begin(), result.end(), result.begin(), [](wchar_t character) {
        return (wchar_t)towlower(character);
    });
    return result;
}

static bool IsWadFile(const std::wstring &name)
{
    if (name.size() < 5) {
        return false;
    }
    return Lowercase(name.substr(name.size() - 4)) == L".wad";
}

static void ScanWadFolder(const std::wstring &folder, std::map<std::wstring, wad_entry_t> &wads)
{
    WIN32_FIND_DATAW data;
    HANDLE search;
    const std::wstring query = folder + L"\\*";

    search = FindFirstFileW(query.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !IsWadFile(data.cFileName)) {
            continue;
        }

        wad_entry_t entry;
        entry.name = data.cFileName;
        entry.path = folder + L"\\" + entry.name;
        wads[Lowercase(entry.name)] = entry;
    } while (FindNextFileW(search, &data));

    FindClose(search);
}

static std::vector<wad_entry_t> FindWads(const wchar_t *local_state)
{
    std::map<std::wstring, wad_entry_t> indexed_wads;
    std::vector<wad_entry_t> wads;

    ScanWadFolder(std::wstring(local_state) + L"\\wads", indexed_wads);
    ScanWadFolder(L"E:\\doom64ex\\wads", indexed_wads);

    for (const auto &pair : indexed_wads) {
        wads.push_back(pair.second);
    }
    std::sort(wads.begin(), wads.end(), [](const wad_entry_t &a, const wad_entry_t &b) {
        return Lowercase(a.name) < Lowercase(b.name);
    });
    return wads;
}

static int GlyphIndex(wchar_t character)
{
    const wchar_t upper = towupper(character);
    if (upper >= L'A' && upper <= L'Z') {
        return (int)(upper - L'A');
    }
    if (upper >= L'0' && upper <= L'9') {
        return 26 + (int)(upper - L'0');
    }
    switch (upper) {
    case L' ': return 36;
    case L'.': return 37;
    case L'-': return 38;
    case L'_': return 39;
    case L'/': return 40;
    case L'+': return 41;
    case L':': return 42;
    case L'>': return 44;
    default: return 43;
    }
}

static int TextWidth(const std::wstring &text)
{
    return (int)text.size() * kGlyphWidth;
}

static void DrawQuad(int x, int y, int width, int height, SDL_Color color)
{
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_QUADS);
    glVertex2i(x, y);
    glVertex2i(x + width, y);
    glVertex2i(x + width, y + height);
    glVertex2i(x, y + height);
    glEnd();
}

static void DrawFrame(int x, int y, int width, int height, SDL_Color color)
{
    glColor4ub(color.r, color.g, color.b, color.a);
    glBegin(GL_LINE_LOOP);
    glVertex2i(x, y);
    glVertex2i(x + width, y);
    glVertex2i(x + width, y + height);
    glVertex2i(x, y + height);
    glEnd();
}

static void DrawTextScaled(const std::wstring &text, int x, int y, int scale, SDL_Color color)
{
    for (wchar_t character : text) {
        const Uint8 *glyph = kGlyphs[GlyphIndex(character)];
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if (glyph[row] & (1 << (4 - column))) {
                    DrawQuad(x + column * scale, y + row * scale, scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
}

static void DrawText(const std::wstring &text, int x, int y, SDL_Color color)
{
    DrawTextScaled(text, x, y, kFontScale, color);
}

static void DrawCenteredText(const std::wstring &text, int center_x, int y, SDL_Color color)
{
    DrawText(text, center_x - TextWidth(text) / 2, y, color);
}

static void DrawReducedCenteredText(const std::wstring &text, int center_x, int y, SDL_Color color)
{
    glPushMatrix();
    glTranslatef((GLfloat)center_x, (GLfloat)y, 0.0f);
    glScalef(0.9f, 0.9f, 1.0f);
    DrawText(text, -TextWidth(text) / 2, 0, color);
    glPopMatrix();
}

static void DrawFilename(const std::wstring &text, int x, int y, int width, SDL_Color color)
{
    const int scale = 1;
    (void)width;
    DrawTextScaled(text, x, y + 4, scale, color);
}

static void ClampListSelection(int &selection, int count)
{
    if (count <= 0) {
        selection = 0;
    }
    else if (selection < 0) {
        selection = count - 1;
    }
    else if (selection >= count) {
        selection = 0;
    }
}

static int ListStart(int selection)
{
    return selection < kListRows ? 0 : selection - kListRows + 1;
}

static void DrawWadList(const std::vector<wad_entry_t> &wads,
        int x, int y, int width, int selection, bool focused, bool ordered)
{
    const SDL_Color white = { 232, 232, 232, 255 };
    const SDL_Color red = { 224, 32, 40, 255 };
    const int start = ListStart(selection);

    for (int row = 0; row < kListRows && start + row < (int)wads.size(); ++row) {
        const int index = start + row;
        const int row_y = y + row * kRowHeight;
        const bool active = focused && index == selection;
        int text_x = x + 8;
        int text_width = width - 16;

        if (active) {
            DrawQuad(x + 2, row_y - 3, width - 4, kRowHeight - 2, { 128, 10, 18, 255 });
            DrawText(L">", x - 8, row_y, red);
        }

        if (ordered) {
            DrawTextScaled(std::to_wstring(index + 1) + L".", text_x, row_y + 4, 1, white);
            text_x += 4 * 6;
            text_width -= 4 * 6;
        }
        DrawFilename(wads[index].name, text_x, row_y, text_width, white);
    }

    if (wads.empty()) {
        const std::wstring message = ordered ? L"NO WADS IN LOAD ORDER" : L"NO WADS FOUND";
        DrawTextScaled(message, x + (width - (int)message.size() * 6) / 2, y + 4, 1, white);
    }
}

static void DrawLauncher(SDL_Window *window, launcher_screen_t screen, int main_selection,
        const std::vector<wad_entry_t> &available, const std::vector<wad_entry_t> &selected,
        int left_selection, int right_selection, bool focused_right)
{
    const SDL_Color white = { 232, 232, 232, 255 };
    const SDL_Color red = { 224, 32, 40, 255 };
    const SDL_Color dark_red = { 64, 4, 10, 255 };

    glClearColor(8.0f / 255.0f, 8.0f / 255.0f, 10.0f / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    DrawQuad(0, 64, kLogicalWidth, 350, { 46, 4, 8, 255 });
    DrawCenteredText(L"DOOM 64 EX", kLogicalWidth / 2, 38, red);

    if (screen == launcher_main) {
        const wchar_t *items[] = { L"START GAME", L"CHOOSE CUSTOM WAD" };
        DrawCenteredText(L"MAIN MENU", kLogicalWidth / 2, 128, white);
        for (int i = 0; i < 2; ++i) {
            const int y = 190 + i * 42;
            if (i == main_selection) {
                DrawQuad(184, y - 5, 280, 30, dark_red);
                DrawTextScaled(L">", 156, y - 1, 3, red);
            }
            DrawCenteredText(items[i], kLogicalWidth / 2, y + 3, i == main_selection ? red : white);
        }
        DrawCenteredText(L"A: SELECT", kLogicalWidth / 2, 440, white);
    }
    else {
        DrawQuad(28, 112, 276, 294, { 34, 5, 9, 255 });
        DrawQuad(336, 112, 276, 294, { 34, 5, 9, 255 });
        DrawFrame(28, 112, 276, 294, red);
        DrawFrame(336, 112, 276, 294, red);
        DrawCenteredText(L"AVAILABLE WADS", 166, 84, focused_right ? white : red);
        DrawCenteredText(L"LOAD ORDER", 474, 84, focused_right ? red : white);
        DrawWadList(available, 38, 128, 256, left_selection, !focused_right, false);
        DrawWadList(selected, 346, 128, 256, right_selection, focused_right, true);
        DrawReducedCenteredText(L"A: ADD   X: REMOVE   B: BACK   MENU: START GAME", kLogicalWidth / 2, 426, white);
        DrawReducedCenteredText(L"VIEW + DPAD: ORDER", kLogicalWidth / 2, 452, white);
    }

    SDL_GL_SwapWindow(window);
}

static void AddSelectedWad(std::vector<wad_entry_t> &available, int &selection,
        std::vector<wad_entry_t> &selected)
{
    if (available.empty()) {
        return;
    }

    selected.push_back(available[selection]);
    available.erase(available.begin() + selection);
    ClampListSelection(selection, (int)available.size());
}

static void RemoveSelectedWad(std::vector<wad_entry_t> &available,
        std::vector<wad_entry_t> &selected, int &selection)
{
    if (selected.empty()) {
        return;
    }

    available.push_back(selected[selection]);
    std::sort(available.begin(), available.end(), [](const wad_entry_t &a, const wad_entry_t &b) {
        return Lowercase(a.name) < Lowercase(b.name);
    });
    selected.erase(selected.begin() + selection);
    ClampListSelection(selection, (int)selected.size());
}

} // namespace

std::string doom64ex_uwp_narrow_path(const std::wstring &path)
{
    const int source_length = (int)path.size();
    const int length = WideCharToMultiByte(CP_ACP, 0, path.c_str(), source_length, nullptr, 0, nullptr, nullptr);
    std::string result(length > 0 ? length : 0, '\0');
    if (length > 0) {
        WideCharToMultiByte(CP_ACP, 0, path.c_str(), source_length, &result[0], length, nullptr, nullptr);
    }
    return result;
}

bool doom64ex_uwp_run_wad_launcher(const wchar_t *local_state, std::vector<std::wstring> &selected_wads)
{
    launcher_screen_t screen = launcher_main;
    std::vector<wad_entry_t> available = FindWads(local_state);
    std::vector<wad_entry_t> selected;
    SDL_Window *window;
    SDL_GLContext context;
    SDL_GameController *controller = nullptr;
    int main_selection = 0;
    int left_selection = 0;
    int right_selection = 0;
    bool focused_right = false;
    bool view_held = false;
    bool running = true;
    bool launch = false;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        return false;
    }

    if (SDL_GL_LoadLibrary("opengl32.dll") < 0) {
        SDL_Quit();
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_BUFFER_SIZE, 32);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    window = SDL_CreateWindow("Doom64EX Classic", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        kLogicalWidth, kLogicalHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL);
    if (!window) {
        SDL_Quit();
        return false;
    }
    context = SDL_GL_CreateContext(window);
    if (!context) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return false;
    }
    SDL_GL_SetSwapInterval(1);
    glViewport(0, 0, kLogicalWidth, kLogicalHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, kLogicalWidth, kLogicalHeight, 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    for (int index = 0; index < SDL_NumJoysticks() && !controller; ++index) {
        if (SDL_IsGameController(index)) {
            controller = SDL_GameControllerOpen(index);
        }
    }

    while (running && !launch) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            int direction = 0;
            bool activate = false;
            bool back = false;
            bool remove = false;
            bool start = false;

            if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) {
                controller = SDL_GameControllerOpen(event.cdevice.which);
                continue;
            }
            if (event.type == SDL_CONTROLLERDEVICEREMOVED && controller &&
                SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller)) == event.cdevice.which) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
                continue;
            }
            if (event.type == SDL_QUIT) {
                running = false;
                continue;
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                switch (event.key.keysym.sym) {
                case SDLK_UP: direction = -1; break;
                case SDLK_DOWN: direction = 1; break;
                case SDLK_LEFT: focused_right = false; break;
                case SDLK_RIGHT: focused_right = true; break;
                case SDLK_RETURN: activate = true; break;
                case SDLK_ESCAPE: back = true; break;
                case SDLK_DELETE: remove = true; break;
                case SDLK_SPACE: start = true; break;
                default: break;
                }
            }
            else if (event.type == SDL_CONTROLLERBUTTONDOWN) {
                switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_DPAD_UP: direction = -1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN: direction = 1; break;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT: focused_right = false; break;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: focused_right = true; break;
                case SDL_CONTROLLER_BUTTON_A: activate = true; break;
                case SDL_CONTROLLER_BUTTON_B: back = true; break;
                case SDL_CONTROLLER_BUTTON_X: remove = true; break;
                case SDL_CONTROLLER_BUTTON_START: start = true; break;
                case SDL_CONTROLLER_BUTTON_BACK: view_held = true; break;
                default: break;
                }
            }
            else if (event.type == SDL_CONTROLLERBUTTONUP && event.cbutton.button == SDL_CONTROLLER_BUTTON_BACK) {
                view_held = false;
            }

            if (screen == launcher_main) {
                if (direction) {
                    main_selection = (main_selection + direction + 2) % 2;
                }
                if (activate || start) {
                    if (main_selection == 0 || start) {
                        launch = true;
                    }
                    else {
                        screen = launcher_wads;
                        focused_right = false;
                    }
                }
                if (back) {
                    running = false;
                }
                continue;
            }

            if (direction) {
                if (focused_right) {
                    if (view_held && !selected.empty()) {
                        const int destination = right_selection + direction;
                        if (destination >= 0 && destination < (int)selected.size()) {
                            std::swap(selected[right_selection], selected[destination]);
                            right_selection = destination;
                        }
                    }
                    else {
                        right_selection += direction;
                        ClampListSelection(right_selection, (int)selected.size());
                    }
                }
                else {
                    left_selection += direction;
                    ClampListSelection(left_selection, (int)available.size());
                }
            }
            if (activate && !focused_right && !available.empty()) {
                AddSelectedWad(available, left_selection, selected);
                right_selection = (int)selected.size() - 1;
            }
            if (remove && focused_right && !selected.empty()) {
                RemoveSelectedWad(available, selected, right_selection);
            }
            if (start) {
                launch = true;
            }
            if (back) {
                screen = launcher_main;
                focused_right = false;
            }
        }

        int drawable_width;
        int drawable_height;
        SDL_GL_GetDrawableSize(window, &drawable_width, &drawable_height);
        const float horizontal_scale = (float)drawable_width / (float)kLogicalWidth;
        const float vertical_scale = (float)drawable_height / (float)kLogicalHeight;
        const float scale = horizontal_scale < vertical_scale ? horizontal_scale : vertical_scale;
        const int viewport_width = (int)(kLogicalWidth * scale);
        const int viewport_height = (int)(kLogicalHeight * scale);
        glViewport((drawable_width - viewport_width) / 2, (drawable_height - viewport_height) / 2,
            viewport_width, viewport_height);

        DrawLauncher(window, screen, main_selection, available, selected,
            left_selection, right_selection, focused_right);
        SDL_Delay(10);
    }

    if (controller) {
        SDL_GameControllerClose(controller);
    }
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);

    if (!launch) {
        return false;
    }
    for (const wad_entry_t &wad : selected) {
        selected_wads.push_back(wad.path);
    }
    return true;
}
