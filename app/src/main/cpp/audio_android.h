#ifndef OOMDROID_AUDIO_H
#define OOMDROID_AUDIO_H

#include <stdint.h>

#define AUDIO_RATE 44100

int audio_android_init(void);
void audio_android_mix(int16_t *dst, int frames);
void audio_android_set_paused(int paused);

#endif
