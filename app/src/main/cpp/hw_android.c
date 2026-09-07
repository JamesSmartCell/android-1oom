#include "config.h"

#include <android/log.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "audio_android.h"
#include "hw.h"
#include "hw_android.h"
#include "kbd.h"
#include "mouse.h"
#include "cfg.h"
#include "options.h"
#include "types.h"

#define TAG "oomdroid"
#define NUM_VIDEOBUF 4
#define TAP_MAX_MS 400
#define TAP_MAX_PX 16

const char *idstr_hw = "android";

const struct cmdline_options_s hw_cmdline_options[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

const struct cmdline_options_s hw_cmdline_options_extra[] = {
    { NULL, 0, NULL, NULL, NULL, NULL }
};

const struct cfg_items_s hw_cfg_items[] = {
    CFG_ITEM_END
};

const struct cfg_items_s hw_cfg_items_extra[] = {
    CFG_ITEM_END
};

static struct {
    uint8_t *buf[NUM_VIDEOBUF];
    int bufw;
    int bufh;
    int bufi;
    uint8_t pal6[256 * 3];
    uint32_t palargb[256];
    uint32_t *present;
    bool present_ready;
} video;

static pthread_mutex_t video_mux = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t input_mux = PTHREAD_MUTEX_INITIALIZER;

static int touch_x, touch_y;
static bool touch_down;
static bool touch_have;
static int key_pending;
static int key_char;
static bool key_down_pending;
static volatile int s_text_wanted;
static bool s_ignore_until_up;

static uint8_t vga6_to_8(uint8_t c)
{
    return (uint8_t)((c << 2) | ((c >> 4) & 3));
}

static uint32_t rgb6_to_argb(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t r8 = vga6_to_8(r);
    uint8_t g8 = vga6_to_8(g);
    uint8_t b8 = vga6_to_8(b);
    /* ANDROID_BITMAP_FORMAT_RGBA_8888 is R,G,B,A in memory. */
    return (uint32_t)r8 | ((uint32_t)g8 << 8) | ((uint32_t)b8 << 16) | 0xff000000u;
}

static void rebuild_pal_range(int first, int num)
{
    for (int i = 0; i < num; ++i) {
        int idx = first + i;
        const uint8_t *c = &video.pal6[idx * 3];
        video.palargb[idx] = rgb6_to_argb(c[0], c[1], c[2]);
    }
}

static void present_and_blit(int front)
{
    if (!video.buf[0] || !video.present) {
        return;
    }
    const uint8_t *src = video.buf[video.bufi ^ front];
    const int w = video.bufw;
    const int h = video.bufh;
    pthread_mutex_lock(&video_mux);
    for (int y = 0; y < h; ++y) {
        const uint8_t *row = src + y * w;
        uint32_t *dst = video.present + y * w;
        for (int x = 0; x < w; ++x) {
            dst[x] = video.palargb[row[x]];
        }
    }
    video.present_ready = true;
    pthread_mutex_unlock(&video_mux);
}

void hw_opt_menu_make_page_video(void)
{
}

int hw_early_init(void)
{
    return 0;
}

int hw_init(void)
{
    memset(&video, 0, sizeof(video));
    if (audio_android_init() != 0) {
        __android_log_print(ANDROID_LOG_WARN, TAG, "audio init failed");
    }
    return 0;
}

void hw_shutdown(void)
{
    for (int i = 0; i < NUM_VIDEOBUF; ++i) {
        free(video.buf[i]);
        video.buf[i] = NULL;
    }
    free(video.present);
    video.present = NULL;
}

void hw_log_message(const char *msg)
{
    __android_log_print(ANDROID_LOG_INFO, TAG, "%s", msg ? msg : "");
}

void hw_log_warning(const char *msg)
{
    __android_log_print(ANDROID_LOG_WARN, TAG, "%s", msg ? msg : "");
}

void hw_log_error(const char *msg)
{
    __android_log_print(ANDROID_LOG_ERROR, TAG, "%s", msg ? msg : "");
}

void hw_android_pointer(int x, int y, bool down)
{
    pthread_mutex_lock(&input_mux);
    touch_x = x;
    touch_y = y;
    touch_down = down;
    touch_have = true;
    pthread_mutex_unlock(&input_mux);
}

void hw_android_key(int key, int c, bool down)
{
    pthread_mutex_lock(&input_mux);
    key_pending = key;
    key_char = c;
    key_down_pending = down;
    pthread_mutex_unlock(&input_mux);
}

static void pump_input(void)
{
    int x, y, key, ch;
    bool down, have, kdown;

    pthread_mutex_lock(&input_mux);
    x = touch_x;
    y = touch_y;
    down = touch_down;
    have = touch_have;
    key = key_pending;
    ch = key_char;
    kdown = key_down_pending;
    key_pending = 0;
    pthread_mutex_unlock(&input_mux);

    if (key) {
        kbd_add_keypress((mookey_t)key, 0, (char)ch);
        kbd_set_pressed((mookey_t)key, 0, kdown);
        if (!kdown) {
            kbd_set_pressed((mookey_t)key, 0, false);
        }
    }

    if (!have || video.bufw <= 0 || video.bufh <= 0) {
        return;
    }

    static bool was_down;
    static bool click_next;
    static int start_x, start_y, last_x, last_y;
    static int64_t start_us;

    if (s_ignore_until_up) {
        if (down) {
            hw_mouse_set_xy(x, y);
        } else {
            s_ignore_until_up = false;
            was_down = false;
            click_next = false;
            mouse_set_buttons_from_hw(0);
        }
        return;
    }

    if (click_next) {
        mouse_set_buttons_from_hw(0);
        click_next = false;
    }

    if (down) {
        last_x = x;
        last_y = y;
        hw_mouse_set_xy(x, y);
        if (!was_down) {
            start_x = x;
            start_y = y;
            start_us = hw_get_time_us();
        }
    } else if (was_down) {
        int64_t dur_ms = (hw_get_time_us() - start_us) / 1000;
        int dx = last_x - start_x;
        int dy = last_y - start_y;
        if (dx < 0) {
            dx = -dx;
        }
        if (dy < 0) {
            dy = -dy;
        }
        if (dur_ms < TAP_MAX_MS && dx < TAP_MAX_PX && dy < TAP_MAX_PX) {
            hw_mouse_set_xy(last_x, last_y);
            mouse_set_buttons_from_hw(MOUSE_BUTTON_MASK_LEFT);
            click_next = true;
        }
    }
    was_down = down;
}

int hw_event_handle(void)
{
    pump_input();
    usleep(8000);
    return 0;
}

void hw_textinput_start(void)
{
    s_text_wanted = 1;
    s_ignore_until_up = true;
    mouse_set_buttons_from_hw(0);
    mouse_getclear_click_hw();
    mouse_getclear_click_sw();
}

void hw_textinput_stop(void)
{
    s_text_wanted = 0;
}

int hw_android_text_input_wanted(void)
{
    return s_text_wanted;
}

bool hw_kbd_set_repeat(bool enabled)
{
    (void)enabled;
    return true;
}

void hw_mouse_set_xy(int mx, int my)
{
    if (mx < 0) {
        mx = 0;
    }
    if (my < 0) {
        my = 0;
    }
    if (video.bufw && mx >= video.bufw) {
        mx = video.bufw - 1;
    }
    if (video.bufh && my >= video.bufh) {
        my = video.bufh - 1;
    }
    mouse_set_xy_from_hw(mx, my);
}

int hw_icon_set(const uint8_t *data, const uint8_t *pal, int w, int h)
{
    (void)data;
    (void)pal;
    (void)w;
    (void)h;
    return 0;
}

int hw_video_init(int w, int h)
{
    if (w <= 0 || h <= 0) {
        return 1;
    }
    if (video.buf[0] && video.bufw == w && video.bufh == h) {
        return 0;
    }
    size_t nbytes = (size_t)w * (size_t)h;
    for (int i = 0; i < NUM_VIDEOBUF; ++i) {
        free(video.buf[i]);
        video.buf[i] = calloc(1, nbytes);
        if (!video.buf[i]) {
            return 1;
        }
    }
    pthread_mutex_lock(&video_mux);
    free(video.present);
    video.present = calloc(nbytes, sizeof(uint32_t));
    video.bufw = w;
    video.bufh = h;
    video.bufi = 0;
    video.present_ready = false;
    pthread_mutex_unlock(&video_mux);
    if (!video.present) {
        return 1;
    }
    hw_mouse_set_xy(w / 2, h / 2);
    __android_log_print(ANDROID_LOG_INFO, TAG, "video %dx%d", w, h);
    return 0;
}

void hw_video_set_palette(const uint8_t *palette, int first, int num)
{
    if (!palette || first < 0 || num <= 0 || first + num > 256) {
        return;
    }
    memcpy(&video.pal6[first * 3], palette, (size_t)num * 3);
    rebuild_pal_range(first, num);
}

void hw_video_set_palette_color(int i, uint8_t r, uint8_t g, uint8_t b)
{
    if (i < 0 || i > 255) {
        return;
    }
    video.pal6[i * 3 + 0] = r;
    video.pal6[i * 3 + 1] = g;
    video.pal6[i * 3 + 2] = b;
    video.palargb[i] = rgb6_to_argb(r, g, b);
}

void hw_video_refresh_palette(void)
{
    rebuild_pal_range(0, 256);
}

uint8_t *hw_video_get_buf(void)
{
    return video.buf[video.bufi];
}

uint8_t *hw_video_get_buf_front(void)
{
    return video.buf[video.bufi ^ 1];
}

uint8_t *hw_video_draw_buf(void)
{
    present_and_blit(0);
    video.bufi ^= 1;
    return video.buf[video.bufi];
}

void hw_video_redraw_front(void)
{
    present_and_blit(1);
}

void hw_video_copy_buf(void)
{
    if (video.buf[0]) {
        memcpy(video.buf[video.bufi], video.buf[video.bufi ^ 1], (size_t)video.bufw * video.bufh);
    }
}

void hw_video_copy_buf_out(uint8_t *buf)
{
    if (video.buf[0] && buf) {
        memcpy(buf, video.buf[video.bufi], (size_t)video.bufw * video.bufh);
    }
}

void hw_video_copy_back_to_page2(void)
{
    if (video.buf[0]) {
        memcpy(video.buf[2], video.buf[video.bufi], (size_t)video.bufw * video.bufh);
    }
}

void hw_video_copy_back_from_page2(void)
{
    if (video.buf[0]) {
        memcpy(video.buf[video.bufi], video.buf[2], (size_t)video.bufw * video.bufh);
    }
}

void hw_video_copy_back_to_page3(void)
{
    if (video.buf[0]) {
        memcpy(video.buf[3], video.buf[video.bufi], (size_t)video.bufw * video.bufh);
    }
}

void hw_video_copy_back_from_page3(void)
{
    if (video.buf[0]) {
        memcpy(video.buf[video.bufi], video.buf[3], (size_t)video.bufw * video.bufh);
    }
}

int64_t hw_get_time_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000ll + ts.tv_nsec / 1000;
}

int hw_android_video_size(int *w, int *h)
{
    pthread_mutex_lock(&video_mux);
    int ok = video.present_ready && video.bufw > 0 && video.bufh > 0;
    if (ok) {
        *w = video.bufw;
        *h = video.bufh;
    }
    pthread_mutex_unlock(&video_mux);
    return ok ? 0 : 1;
}

int hw_android_copy_argb(uint32_t *dst, int dstw, int dsth)
{
    pthread_mutex_lock(&video_mux);
    if (!video.present_ready || !video.present || dstw != video.bufw || dsth != video.bufh) {
        pthread_mutex_unlock(&video_mux);
        return 1;
    }
    memcpy(dst, video.present, (size_t)dstw * (size_t)dsth * sizeof(uint32_t));
    pthread_mutex_unlock(&video_mux);
    return 0;
}
