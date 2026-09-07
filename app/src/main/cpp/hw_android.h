#ifndef OOMDROID_HW_ANDROID_H
#define OOMDROID_HW_ANDROID_H

#include <stdbool.h>
#include <stdint.h>

void hw_android_pointer(int x, int y, bool down);
void hw_android_key(int key, int c, bool down);
int hw_android_video_size(int *w, int *h);
int hw_android_copy_argb(uint32_t *dst, int dstw, int dsth);
int hw_android_text_input_wanted(void);

#endif
