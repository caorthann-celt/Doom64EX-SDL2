#include <Windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

using namespace Windows::Storage;

extern "C" int I_Main(int argc, char *argv[]);

extern "C" int SDL_main(int argc, char **argv)
{
    static char application_name[] = "Doom64EXClassicUWP";
    static char *safe_argv[] = { application_name, nullptr };

    SDL_SetMainReady();

    const auto local_state = ApplicationData::Current->LocalFolder->Path;
    if (!SetCurrentDirectoryW(local_state->Data())) {
        return 1;
    }

    if (argc <= 0 || !argv || !argv[0]) {
        return I_Main(1, safe_argv);
    }
    return I_Main(argc, argv);
}
