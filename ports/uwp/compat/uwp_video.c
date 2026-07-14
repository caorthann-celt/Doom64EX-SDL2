#include "uwp_video.h"
#include "fluidlite_sdl_audio.h"

void doom64ex_uwp_after_present(SDL_Window *window)
{
    (void)window;
    fluidlite_sdl_audio_activate();
}
