package com.tallydigital.oomdroid

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioTrack
import kotlin.concurrent.thread
import kotlin.math.max

class GameAudio {
    @Volatile
    private var running = false
    @Volatile
    private var paused = false
    private var worker: Thread? = null

    fun start() {
        if (running) {
            return
        }
        paused = false
        GameNative.setAudioPaused(false)
        running = true
        worker = thread(name = "oom-audio", isDaemon = true) {
            val rate = 44100
            val minBuf = AudioTrack.getMinBufferSize(
                rate,
                AudioFormat.CHANNEL_OUT_STEREO,
                AudioFormat.ENCODING_PCM_16BIT,
            )
            val track = AudioTrack.Builder()
                .setAudioAttributes(
                    AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_GAME)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                        .build(),
                )
                .setAudioFormat(
                    AudioFormat.Builder()
                        .setSampleRate(rate)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                        .build(),
                )
                .setBufferSizeInBytes(max(minBuf, 4096))
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build()
            val mix = ShortArray(512)
            track.play()
            try {
                while (running) {
                    if (paused) {
                        track.pause()
                        while (running && paused) {
                            Thread.sleep(50)
                        }
                        if (running) {
                            track.play()
                        }
                        continue
                    }
                    GameNative.mixAudio(mix)
                    track.write(mix, 0, mix.size)
                }
            } finally {
                track.stop()
                track.release()
            }
        }
    }

    fun pause() {
        paused = true
        GameNative.setAudioPaused(true)
    }

    fun resume() {
        if (!running) {
            return
        }
        GameNative.setAudioPaused(false)
        paused = false
    }

    fun stop() {
        paused = false
        running = false
        worker = null
    }
}
