#ifndef DOOM64EX_UWP_FLUIDSYNTH_H
#define DOOM64EX_UWP_FLUIDSYNTH_H

#include <fluidlite.h>

#ifdef __cplusplus
extern "C" {
#endif

fluid_audio_driver_t *new_fluid_audio_driver(fluid_settings_t *settings, fluid_synth_t *synth);
void delete_fluid_audio_driver(fluid_audio_driver_t *driver);

#ifdef __cplusplus
}
#endif

#endif
