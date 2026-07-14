#include <Windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>

#include <string>
#include <vector>

#include "uwp_wad_launcher.h"

using namespace Windows::Storage;

extern "C" int I_Main(int argc, char *argv[]);

extern "C" int SDL_main(int argc, char **argv)
{
    static char application_name[] = "Doom64EXClassicUWP";
    std::vector<std::wstring> selected_wads;
    std::vector<std::string> game_arguments;
    std::vector<char *> game_argv;

    SDL_SetMainReady();

    const auto local_state = ApplicationData::Current->LocalFolder->Path;
    if (!SetCurrentDirectoryW(local_state->Data())) {
        return 1;
    }

    if (!doom64ex_uwp_run_wad_launcher(local_state->Data(), selected_wads)) {
        SDL_Quit();
        return 0;
    }

    game_arguments.emplace_back(application_name);
    if (!selected_wads.empty()) {
        game_arguments.emplace_back("-file");
        for (const std::wstring &path : selected_wads) {
            game_arguments.push_back(doom64ex_uwp_narrow_path(path));
        }
    }
    for (std::string &argument : game_arguments) {
        game_argv.push_back(&argument[0]);
    }
    game_argv.push_back(nullptr);

    SDL_Quit();
    return I_Main((int)game_arguments.size(), game_argv.data());
}
