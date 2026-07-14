#include <Windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

extern "C" int SDL_main(int argc, char **argv);

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return SDL_WinRTRunApp(SDL_main, nullptr);
}
