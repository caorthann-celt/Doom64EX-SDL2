#include "fluidsynth.h"
#include "fluidlite_sdl_audio.h"

#include <SDL2/SDL.h>

struct _fluid_audio_driver_t {
    fluid_synth_t *synth;
    SDL_AudioDeviceID device;
    SDL_Thread *open_thread;
    SDL_atomic_t closing;
};

static fluid_audio_driver_t *pending_driver;

static void SDLCALL fluidlite_audio_callback(void *userdata, Uint8 *stream, int length)
{
    fluid_audio_driver_t *driver = (fluid_audio_driver_t *)userdata;
    const int frames = length / (int)(sizeof(Sint16) * 2);

    if (!driver || !driver->synth || frames <= 0) {
        SDL_memset(stream, 0, (size_t)length);
        return;
    }
    fluid_synth_write_s16(driver->synth, frames, (Sint16 *)stream, 0, 2, (Sint16 *)stream, 1, 2);
}

static int SDLCALL fluidlite_open_audio_device(void *userdata)
{
    fluid_audio_driver_t *driver = (fluid_audio_driver_t *)userdata;
    SDL_AudioSpec desired;

    SDL_zero(desired);
    desired.freq = 44100;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = fluidlite_audio_callback;
    desired.userdata = driver;

    while (!SDL_AtomicGet(&driver->closing)) {
        driver->device = SDL_OpenAudioDevice(NULL, 0, &desired, NULL, 0);
        if (driver->device != 0) {
            SDL_PauseAudioDevice(driver->device, 0);
            return 0;
        }
        SDL_Delay(250);
    }
    return 0;
}

fluid_audio_driver_t *new_fluid_audio_driver(fluid_settings_t *settings, fluid_synth_t *synth)
{
    fluid_audio_driver_t *driver;
    (void)settings;
    if (!synth || (!(SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)) {
        return NULL;
    }
    driver = (fluid_audio_driver_t *)SDL_calloc(1, sizeof(*driver));
    if (!driver) {
        return NULL;
    }
    fluid_synth_set_sample_rate(synth, 44100.0f);
    driver->synth = synth;
    SDL_AtomicSet(&driver->closing, 0);
    pending_driver = driver;
    return driver;
}

void fluidlite_sdl_audio_activate(void)
{
    if (pending_driver && !pending_driver->open_thread) {
        pending_driver->open_thread = SDL_CreateThread(fluidlite_open_audio_device, "FluidLiteAudio", pending_driver);
    }
}

void delete_fluid_audio_driver(fluid_audio_driver_t *driver)
{
    if (!driver) return;
    if (pending_driver == driver) pending_driver = NULL;
    SDL_AtomicSet(&driver->closing, 1);
    if (driver->open_thread) SDL_WaitThread(driver->open_thread, NULL);
    if (driver->device != 0) SDL_CloseAudioDevice(driver->device);
    SDL_free(driver);
}
