#include <jni.h>
#include <android/bitmap.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "audio_android.h"
#include "hw_android.h"
#include "main.h"
#include "uiobj.h"

static char *dup_jstr(JNIEnv *env, jstring s)
{
    if (!s) {
        return NULL;
    }
    const char *p = (*env)->GetStringUTFChars(env, s, NULL);
    if (!p) {
        return NULL;
    }
    char *out = strdup(p);
    (*env)->ReleaseStringUTFChars(env, s, p);
    return out;
}

JNIEXPORT jint JNICALL
Java_com_tallydigital_oomdroid_GameNative_start(JNIEnv *env, jobject thiz, jstring data_path, jstring user_path)
{
    (void)thiz;
    char *data = dup_jstr(env, data_path);
    char *user = dup_jstr(env, user_path);
    if (!data || !user) {
        free(data);
        free(user);
        return 1;
    }
    char *argv[] = {
        "1oom",
        "-data", data,
        "-user", user,
        "-nolog",
        "-cro",
        "-skipintro",
        "-nosfxinitpar",
        "-musicvol", "8",
        "-uiscale", "1",
        NULL
    };
    int argc = (int)(sizeof(argv) / sizeof(argv[0])) - 1;
    int rc = main_1oom(argc, argv);
    free(data);
    free(user);
    return rc;
}

JNIEXPORT void JNICALL
Java_com_tallydigital_oomdroid_GameNative_onTouch(JNIEnv *env, jobject thiz, jint x, jint y, jboolean down)
{
    (void)env;
    (void)thiz;
    hw_android_pointer(x, y, down == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tallydigital_oomdroid_GameNative_onKey(JNIEnv *env, jobject thiz, jint key, jint ch, jboolean down)
{
    (void)env;
    (void)thiz;
    hw_android_key(key, ch, down == JNI_TRUE);
}

JNIEXPORT jintArray JNICALL
Java_com_tallydigital_oomdroid_GameNative_videoSize(JNIEnv *env, jobject thiz)
{
    (void)thiz;
    int w = 0, h = 0;
    jintArray arr = (*env)->NewIntArray(env, 2);
    if (!arr) {
        return NULL;
    }
    if (hw_android_video_size(&w, &h) != 0) {
        return arr;
    }
    jint wh[2] = { w, h };
    (*env)->SetIntArrayRegion(env, arr, 0, 2, wh);
    return arr;
}

JNIEXPORT jboolean JNICALL
Java_com_tallydigital_oomdroid_GameNative_copyFrame(JNIEnv *env, jobject thiz, jobject bitmap)
{
    (void)thiz;
    AndroidBitmapInfo info;
    void *pixels = NULL;
    if (AndroidBitmap_getInfo(env, bitmap, &info) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return JNI_FALSE;
    }
    if (info.format != ANDROID_BITMAP_FORMAT_RGBA_8888) {
        return JNI_FALSE;
    }
    if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != ANDROID_BITMAP_RESULT_SUCCESS) {
        return JNI_FALSE;
    }
    int rc = hw_android_copy_argb((uint32_t *)pixels, (int)info.width, (int)info.height);
    AndroidBitmap_unlockPixels(env, bitmap);
    return rc == 0 ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_com_tallydigital_oomdroid_GameNative_textInputWanted(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    return hw_android_text_input_wanted() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jint JNICALL
Java_com_tallydigital_oomdroid_GameNative_listScrollState(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    return uiobj_list_scroll_state();
}

JNIEXPORT jintArray JNICALL
Java_com_tallydigital_oomdroid_GameNative_overlayLayout(JNIEnv *env, jobject thiz)
{
    jint vals[13];
    jintArray arr;
    int i;

    (void)thiz;
    vals[0] = uiobj_list_scroll_state();
    for (i = 0; i < 3; ++i) {
        int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        uiobj_list_overlay_rect(i, &x0, &y0, &x1, &y1);
        vals[1 + i * 4] = x0;
        vals[2 + i * 4] = y0;
        vals[3 + i * 4] = x1;
        vals[4 + i * 4] = y1;
    }
    arr = (*env)->NewIntArray(env, 13);
    if (!arr) {
        return NULL;
    }
    (*env)->SetIntArrayRegion(env, arr, 0, 13, vals);
    return arr;
}

JNIEXPORT void JNICALL
Java_com_tallydigital_oomdroid_GameNative_listScrollPage(JNIEnv *env, jobject thiz, jint dir)
{
    (void)env;
    (void)thiz;
    uiobj_list_page(dir < 0 ? -1 : 1);
}

JNIEXPORT void JNICALL
Java_com_tallydigital_oomdroid_GameNative_listJumpMax(JNIEnv *env, jobject thiz)
{
    (void)env;
    (void)thiz;
    uiobj_list_jump_max();
}

JNIEXPORT void JNICALL
Java_com_tallydigital_oomdroid_GameNative_setAudioPaused(JNIEnv *env, jobject thiz, jboolean paused)
{
    (void)env;
    (void)thiz;
    audio_android_set_paused(paused == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_tallydigital_oomdroid_GameNative_mixAudio(JNIEnv *env, jobject thiz, jshortArray out)
{
    (void)thiz;
    jsize n = (*env)->GetArrayLength(env, out);
    jshort *p = (*env)->GetShortArrayElements(env, out, NULL);
    if (!p) {
        return;
    }
    audio_android_mix((int16_t *)p, n / 2);
    (*env)->ReleaseShortArrayElements(env, out, p, 0);
}
