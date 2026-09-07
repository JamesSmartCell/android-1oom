#include "audio_android.h"

#include <math.h>
#include <pthread.h>
#include <string.h>

#include "fmt_mus.h"
#include "fmt_sfx.h"
#include "hw.h"
#include "lib.h"
#include "options.h"

#define MIX_FRAMES 256
#define SFX_MAX    48
#define MUS_VOICES 12

struct sfx_slot {
    int16_t *pcm;
    uint32_t frames;
};

struct mus_slot {
    uint8_t *smf;
    uint32_t len;
    bool loops;
};

struct voice {
    uint32_t phase;
    uint32_t step;
    int vol;
    uint8_t ch;
    uint8_t note;
    bool on;
};

static bool s_ready;
static volatile int s_paused;
static pthread_mutex_t s_mux = PTHREAD_MUTEX_INITIALIZER;

static struct sfx_slot s_sfx[SFX_MAX];
static int s_sfx_n;
static int s_sfx_play = -1;
static uint32_t s_sfx_pos;

static struct mus_slot s_mus[4];
static int s_mus_n;
static int s_mus_play = -1;
static int s_mus_fade;
static const uint8_t *s_midi_p;
static const uint8_t *s_midi_end;
static uint8_t s_midi_run;
static uint32_t s_midi_tempo = 500000;
static uint16_t s_midi_div = 60;
static uint32_t s_midi_samples_left;
static bool s_midi_active;
static struct voice s_voice[MUS_VOICES];
static int16_t s_sine[256];
static uint32_t s_note_step[128];

static uint32_t read_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static uint16_t read_be16(const uint8_t *p)
{
    return (uint16_t)((p[0] << 8) | p[1]);
}

static uint32_t read_vlq(const uint8_t **pp, const uint8_t *end)
{
    uint32_t v = 0;
    const uint8_t *p = *pp;
    for (int i = 0; i < 4 && p < end; ++i) {
        uint8_t b = *p++;
        v = (v << 7) | (b & 0x7f);
        if ((b & 0x80) == 0) {
            break;
        }
    }
    *pp = p;
    return v;
}

static void voice_off_note(uint8_t ch, uint8_t note)
{
    for (int i = 0; i < MUS_VOICES; ++i) {
        if (s_voice[i].on && s_voice[i].ch == ch && s_voice[i].note == note) {
            s_voice[i].on = false;
        }
    }
}

static void voice_on(uint8_t ch, uint8_t note, uint8_t vel)
{
    if (ch == 9 || vel == 0) {
        if (vel == 0) {
            voice_off_note(ch, note);
        }
        return;
    }
    int slot = -1;
    for (int i = 0; i < MUS_VOICES; ++i) {
        if (!s_voice[i].on) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        slot = 0;
    }
    s_voice[slot].on = true;
    s_voice[slot].ch = ch;
    s_voice[slot].note = note;
    s_voice[slot].step = s_note_step[note];
    s_voice[slot].vol = (int)vel * 18;
    s_voice[slot].phase = 0;
}

static void midi_reset_playhead(int mus_index)
{
    const uint8_t *p = s_mus[mus_index].smf;
    uint32_t len = s_mus[mus_index].len;
    s_midi_active = false;
    if (!p || len < 22 || memcmp(p, "MThd", 4) != 0) {
        return;
    }
    s_midi_div = read_be16(p + 12);
    if (s_midi_div == 0) {
        s_midi_div = 60;
    }
    p += 8 + read_be32(p + 4);
    if (p + 8 > s_mus[mus_index].smf + len || memcmp(p, "MTrk", 4) != 0) {
        return;
    }
    uint32_t tlen = read_be32(p + 4);
    p += 8;
    s_midi_p = p;
    s_midi_end = p + tlen;
    if (s_midi_end > s_mus[mus_index].smf + len) {
        s_midi_end = s_mus[mus_index].smf + len;
    }
    s_midi_run = 0;
    s_midi_tempo = 500000;
    s_midi_samples_left = 0;
    s_midi_active = true;
    memset(s_voice, 0, sizeof(s_voice));
}

static uint32_t ticks_to_samples(uint32_t ticks)
{
    uint64_t num = (uint64_t)ticks * s_midi_tempo * AUDIO_RATE;
    uint64_t den = (uint64_t)s_midi_div * 1000000ull;
    return (uint32_t)(num / den);
}

static void midi_handle_event(uint8_t status, uint8_t d1, uint8_t d2)
{
    uint8_t cmd = status & 0xf0;
    uint8_t ch = status & 0x0f;
    if (cmd == 0x90) {
        if (d2 == 0) {
            voice_off_note(ch, d1);
        } else {
            voice_on(ch, d1, d2);
        }
    } else if (cmd == 0x80) {
        voice_off_note(ch, d1);
    } else if (cmd == 0xb0 && d1 == 123) {
        memset(s_voice, 0, sizeof(s_voice));
    }
}

static void midi_advance_event(void)
{
    if (!s_midi_active || s_midi_p >= s_midi_end) {
        if (s_mus_play >= 0 && s_mus[s_mus_play].loops) {
            midi_reset_playhead(s_mus_play);
            if (s_midi_active && s_midi_p < s_midi_end) {
                uint32_t dt = read_vlq(&s_midi_p, s_midi_end);
                s_midi_samples_left = ticks_to_samples(dt);
            }
        } else {
            s_midi_active = false;
            s_mus_play = -1;
        }
        return;
    }
    uint8_t b = *s_midi_p;
    if (b & 0x80) {
        s_midi_run = b;
        s_midi_p++;
    }
    if (s_midi_run == 0xff) {
        if (s_midi_p + 1 > s_midi_end) {
            s_midi_active = false;
            return;
        }
        uint8_t type = *s_midi_p++;
        uint32_t n = read_vlq(&s_midi_p, s_midi_end);
        if (type == 0x51 && n == 3 && s_midi_p + 3 <= s_midi_end) {
            s_midi_tempo = ((uint32_t)s_midi_p[0] << 16) | ((uint32_t)s_midi_p[1] << 8) | s_midi_p[2];
            if (s_midi_tempo == 0) {
                s_midi_tempo = 500000;
            }
        } else if (type == 0x2f) {
            s_midi_p = s_midi_end;
        }
        s_midi_p += n;
    } else if (s_midi_run == 0xf0 || s_midi_run == 0xf7) {
        uint32_t n = read_vlq(&s_midi_p, s_midi_end);
        s_midi_p += n;
    } else {
        uint8_t cmd = s_midi_run & 0xf0;
        uint8_t d1 = 0, d2 = 0;
        int nd = (cmd == 0xc0 || cmd == 0xd0) ? 1 : 2;
        if (s_midi_p + nd > s_midi_end) {
            s_midi_active = false;
            return;
        }
        d1 = *s_midi_p++;
        if (nd == 2) {
            d2 = *s_midi_p++;
        }
        midi_handle_event(s_midi_run, d1, d2);
    }
    if (s_midi_p < s_midi_end) {
        uint32_t dt = read_vlq(&s_midi_p, s_midi_end);
        s_midi_samples_left = ticks_to_samples(dt);
    } else {
        s_midi_samples_left = 0;
        midi_advance_event();
    }
}

static int16_t mix_music_sample(void)
{
    if (s_mus_play < 0 || !s_midi_active) {
        return 0;
    }
    while (s_midi_samples_left == 0 && s_midi_active) {
        midi_advance_event();
    }
    if (s_midi_samples_left > 0) {
        s_midi_samples_left--;
    }
    int acc = 0;
    for (int i = 0; i < MUS_VOICES; ++i) {
        if (!s_voice[i].on) {
            continue;
        }
        s_voice[i].phase += s_voice[i].step;
        int16_t s = s_sine[s_voice[i].phase >> 24];
        acc += (s * s_voice[i].vol) >> 14;
    }
    int vol = opt_music_volume;
    if (s_mus_fade > 0) {
        vol = (vol * s_mus_fade) / AUDIO_RATE;
        if (--s_mus_fade == 0) {
            s_mus_play = -1;
            s_midi_active = false;
            memset(s_voice, 0, sizeof(s_voice));
        }
    }
    acc = (acc * vol) >> 10;
    if (acc > 32767) {
        acc = 32767;
    }
    if (acc < -32768) {
        acc = -32768;
    }
    return (int16_t)acc;
}

int audio_android_init(void)
{
    if (s_ready) {
        return 0;
    }
    for (int i = 0; i < 256; ++i) {
        s_sine[i] = (int16_t)(sinf((float)i * 6.2831853f / 256.0f) * 30000.0f);
    }
    for (int n = 0; n < 128; ++n) {
        float f = 440.0f * powf(2.0f, ((float)n - 69.0f) / 12.0f);
        s_note_step[n] = (uint32_t)((f / (float)AUDIO_RATE) * 4294967296.0);
    }
    s_ready = true;
    return 0;
}

void audio_android_set_paused(int paused)
{
    s_paused = paused ? 1 : 0;
}

void audio_android_mix(int16_t *dst, int frames)
{
    if (!dst || frames <= 0) {
        return;
    }
    if (!s_ready || s_paused) {
        memset(dst, 0, (size_t)frames * 4);
        return;
    }
    pthread_mutex_lock(&s_mux);
    int sfx_i = s_sfx_play;
    uint32_t sfx_pos = s_sfx_pos;
    const int16_t *sfx = (sfx_i >= 0 && sfx_i < s_sfx_n) ? s_sfx[sfx_i].pcm : NULL;
    uint32_t sfx_n = (sfx_i >= 0 && sfx_i < s_sfx_n) ? s_sfx[sfx_i].frames : 0;
    int sfx_vol = opt_sfx_volume;
    for (int i = 0; i < frames; ++i) {
        int16_t mus = mix_music_sample();
        int l = mus;
        int r = mus;
        if (sfx && sfx_pos < sfx_n) {
            int sl = (sfx[sfx_pos * 2] * sfx_vol) >> 7;
            int sr = (sfx[sfx_pos * 2 + 1] * sfx_vol) >> 7;
            l += sl;
            r += sr;
            sfx_pos++;
        }
        if (l > 32767) {
            l = 32767;
        }
        if (l < -32768) {
            l = -32768;
        }
        if (r > 32767) {
            r = 32767;
        }
        if (r < -32768) {
            r = -32768;
        }
        dst[i * 2] = (int16_t)l;
        dst[i * 2 + 1] = (int16_t)r;
    }
    if (sfx && sfx_pos >= sfx_n) {
        s_sfx_play = -1;
        s_sfx_pos = 0;
    } else {
        s_sfx_pos = sfx_pos;
    }
    pthread_mutex_unlock(&s_mux);
}

int hw_audio_music_init(int mus_index, const uint8_t *data, uint32_t len)
{
    if (!s_ready || !data || len == 0) {
        return 0;
    }
    if (mus_index < 0 || mus_index >= (int)(sizeof(s_mus) / sizeof(s_mus[0]))) {
        return -1;
    }
    uint8_t *smf = NULL;
    uint32_t slen = 0;
    bool loops = false;
    mus_type_t t = fmt_mus_detect(data, len);
    if (t == MUS_TYPE_LBXXMID) {
        if (!fmt_mus_convert_xmid(data, len, &smf, &slen, &loops)) {
            return -1;
        }
    } else if (t == MUS_TYPE_MIDI) {
        smf = lib_malloc(len);
        if (!smf) {
            return -1;
        }
        memcpy(smf, data, len);
        slen = len;
    } else {
        return -1;
    }
    pthread_mutex_lock(&s_mux);
    lib_free(s_mus[mus_index].smf);
    s_mus[mus_index].smf = smf;
    s_mus[mus_index].len = slen;
    s_mus[mus_index].loops = loops;
    if (s_mus_n <= mus_index) {
        s_mus_n = mus_index + 1;
    }
    pthread_mutex_unlock(&s_mux);
    return 0;
}

void hw_audio_music_release(int mus_index)
{
    if (mus_index < 0 || mus_index >= s_mus_n) {
        return;
    }
    pthread_mutex_lock(&s_mux);
    if (s_mus_play == mus_index) {
        s_mus_play = -1;
        s_midi_active = false;
    }
    lib_free(s_mus[mus_index].smf);
    s_mus[mus_index].smf = NULL;
    s_mus[mus_index].len = 0;
    pthread_mutex_unlock(&s_mux);
}

void hw_audio_music_play(int mus_index)
{
    if (!s_ready || !opt_music_enabled || mus_index < 0 || mus_index >= s_mus_n || !s_mus[mus_index].smf) {
        return;
    }
    pthread_mutex_lock(&s_mux);
    s_mus_fade = 0;
    s_mus_play = mus_index;
    midi_reset_playhead(mus_index);
    if (s_midi_active && s_midi_p < s_midi_end) {
        uint32_t dt = read_vlq(&s_midi_p, s_midi_end);
        s_midi_samples_left = ticks_to_samples(dt);
    }
    pthread_mutex_unlock(&s_mux);
}

void hw_audio_music_fadeout(void)
{
    pthread_mutex_lock(&s_mux);
    if (s_mus_play >= 0) {
        s_mus_fade = AUDIO_RATE;
    }
    pthread_mutex_unlock(&s_mux);
}

void hw_audio_music_stop(void)
{
    pthread_mutex_lock(&s_mux);
    s_mus_play = -1;
    s_mus_fade = 0;
    s_midi_active = false;
    memset(s_voice, 0, sizeof(s_voice));
    pthread_mutex_unlock(&s_mux);
}

bool hw_audio_music_volume(int volume)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 128) {
        volume = 128;
    }
    opt_music_volume = volume;
    return true;
}

int hw_audio_sfx_batch_start(int sfx_index_max)
{
    (void)sfx_index_max;
    return 0;
}

int hw_audio_sfx_batch_end(void)
{
    return 0;
}

int hw_audio_sfx_init(int sfx_index, const uint8_t *data, uint32_t len)
{
    if (!s_ready || !data) {
        return 0;
    }
    if (sfx_index < 0 || sfx_index >= SFX_MAX) {
        return -1;
    }
    uint8_t *pcm = NULL;
    uint32_t nbytes = 0;
    if (!fmt_sfx_convert(data, len, &pcm, &nbytes, NULL, AUDIO_RATE, false) || !pcm) {
        return -1;
    }
    pthread_mutex_lock(&s_mux);
    lib_free(s_sfx[sfx_index].pcm);
    s_sfx[sfx_index].pcm = (int16_t *)pcm;
    s_sfx[sfx_index].frames = nbytes / 4;
    if (s_sfx_n <= sfx_index) {
        s_sfx_n = sfx_index + 1;
    }
    pthread_mutex_unlock(&s_mux);
    return 0;
}

void hw_audio_sfx_release(int sfx_index)
{
    if (sfx_index < 0 || sfx_index >= s_sfx_n) {
        return;
    }
    pthread_mutex_lock(&s_mux);
    if (s_sfx_play == sfx_index) {
        s_sfx_play = -1;
    }
    lib_free(s_sfx[sfx_index].pcm);
    s_sfx[sfx_index].pcm = NULL;
    s_sfx[sfx_index].frames = 0;
    pthread_mutex_unlock(&s_mux);
}

void hw_audio_sfx_play(int sfx_index)
{
    if (!s_ready || !opt_sfx_enabled || sfx_index < 0 || sfx_index >= s_sfx_n || !s_sfx[sfx_index].pcm) {
        return;
    }
    pthread_mutex_lock(&s_mux);
    s_sfx_play = sfx_index;
    s_sfx_pos = 0;
    pthread_mutex_unlock(&s_mux);
}

void hw_audio_sfx_stop(void)
{
    pthread_mutex_lock(&s_mux);
    s_sfx_play = -1;
    s_sfx_pos = 0;
    pthread_mutex_unlock(&s_mux);
}

bool hw_audio_sfx_volume(int volume)
{
    if (volume < 0) {
        volume = 0;
    }
    if (volume > 128) {
        volume = 128;
    }
    opt_sfx_volume = volume;
    return true;
}
