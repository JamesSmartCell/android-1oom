package com.tallydigital.oomdroid

import android.graphics.Bitmap

object GameNative {
    init {
        System.loadLibrary("oomdroid")
    }

    @JvmStatic
    external fun start(dataPath: String, userPath: String): Int

    @JvmStatic
    external fun onTouch(x: Int, y: Int, down: Boolean)

    @JvmStatic
    external fun onKey(key: Int, ch: Int, down: Boolean)

    @JvmStatic
    external fun videoSize(): IntArray

    @JvmStatic
    external fun copyFrame(bitmap: Bitmap): Boolean

    @JvmStatic
    external fun mixAudio(out: ShortArray)

    @JvmStatic
    external fun listScrollState(): Int

    @JvmStatic
    external fun overlayLayout(): IntArray

    @JvmStatic
    external fun listScrollPage(dir: Int)

    @JvmStatic
    external fun listJumpMax()

    @JvmStatic
    external fun setAudioPaused(paused: Boolean)

    @JvmStatic
    external fun textInputWanted(): Boolean
}
