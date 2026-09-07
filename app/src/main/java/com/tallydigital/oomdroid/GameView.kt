package com.tallydigital.oomdroid

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.Rect
import android.graphics.RectF
import android.graphics.Typeface
import android.text.InputType
import android.util.AttributeSet
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.inputmethod.BaseInputConnection
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputConnection
import android.view.inputmethod.InputMethodManager
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import kotlin.math.min

class GameView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
) : View(context, attrs) {

    var onFirstFrame: (() -> Unit)? = null

    private val paint = Paint().apply {
        isFilterBitmap = false
        isAntiAlias = false
        isDither = false
    }
    private val srcRect = Rect()
    private val dstRect = Rect()
    private val upRect = RectF()
    private val dnRect = RectF()
    private val maxRect = RectF()
    private val chevron = Path()
    private val overlayFill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val overlayText = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        textAlign = Paint.Align.CENTER
        typeface = ResourcesCompat.getFont(context, R.font.antonio) ?: Typeface.DEFAULT_BOLD
    }
    private val lcarsGold = ContextCompat.getColor(context, R.color.lcars_gold)
    private val lcarsOrange = ContextCompat.getColor(context, R.color.lcars_orange)
    private val lcarsAlmond = ContextCompat.getColor(context, R.color.lcars_almond)
    private val lcarsGray = ContextCompat.getColor(context, R.color.lcars_gray)
    private val lcarsBlack = ContextCompat.getColor(context, R.color.lcars_black)
    private val lcarsButter = ContextCompat.getColor(context, R.color.lcars_butter)
    private var bitmap = Bitmap.createBitmap(320, 200, Bitmap.Config.ARGB_8888)
    private var gameW = 320
    private var gameH = 200
    private var hasFrame = false
    private var keyboardShown = false
    private var scrollState = 0
    private var scrollGrab = 0
    private var overlayGameRects = IntArray(13)

    init {
        isFocusable = true
        isFocusableInTouchMode = true
        isClickable = true
    }

    fun tick() {
        val size = GameNative.videoSize()
        if (size.size >= 2 && size[0] > 0 && size[1] > 0) {
            if (size[0] != gameW || size[1] != gameH) {
                gameW = size[0]
                gameH = size[1]
                bitmap.recycle()
                bitmap = Bitmap.createBitmap(gameW, gameH, Bitmap.Config.ARGB_8888)
            }
            if (GameNative.copyFrame(bitmap)) {
                if (!hasFrame) {
                    hasFrame = true
                    onFirstFrame?.invoke()
                }
                invalidate()
            }
        }
        val want = GameNative.textInputWanted()
        if (want && !keyboardShown) {
            showIme()
        } else if (!want && keyboardShown) {
            hideIme()
        }
        val layout = GameNative.overlayLayout()
        if (layout.size >= 13 && !layout.contentEquals(overlayGameRects)) {
            overlayGameRects = layout
            scrollState = layout[0]
            invalidate()
        }
        postOnAnimation { tick() }
    }

    override fun onCheckIsTextEditor(): Boolean = true

    override fun onCreateInputConnection(outAttrs: EditorInfo): InputConnection {
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT or InputType.TYPE_TEXT_FLAG_CAP_CHARACTERS or
            InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI or EditorInfo.IME_FLAG_NO_FULLSCREEN or
            EditorInfo.IME_ACTION_DONE
        outAttrs.initialSelStart = 0
        outAttrs.initialSelEnd = 0
        return GameInputConnection(this)
    }

    override fun onDraw(canvas: Canvas) {
        canvas.drawColor(0xff000000.toInt())
        if (!hasFrame) {
            return
        }
        letterbox(width, height, gameW, gameH, dstRect)
        layoutOverlayButtons()
        srcRect.set(0, 0, gameW, gameH)
        canvas.drawBitmap(bitmap, srcRect, dstRect, paint)
        if (scrollState and SCROLL_ACTIVE != 0) {
            drawLcarsButton(canvas, upRect, lcarsGold, enabled = scrollState and SCROLL_UP != 0)
            drawChevron(canvas, upRect, up = true, enabled = scrollState and SCROLL_UP != 0)
            drawLcarsButton(canvas, dnRect, lcarsOrange, enabled = scrollState and SCROLL_DOWN != 0)
            drawChevron(canvas, dnRect, up = false, enabled = scrollState and SCROLL_DOWN != 0)
        }
        if (scrollState and SCROLL_MAX != 0) {
            drawLcarsButton(canvas, maxRect, lcarsAlmond, enabled = true)
            drawMaxLabel(canvas, maxRect)
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (handleScrollTouch(event)) {
            return true
        }
        val gx = viewToGameX(event.x)
        val gy = viewToGameY(event.y)
        when (event.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_MOVE -> {
                GameNative.onTouch(gx, gy, true)
                parent.requestDisallowInterceptTouchEvent(true)
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                GameNative.onTouch(gx, gy, false)
            }
        }
        return true
    }

    private fun showIme() {
        keyboardShown = true
        requestFocus()
        val imm = inputMethodManager()
        imm.restartInput(this)
        post {
            requestFocus()
            imm.showSoftInput(this, InputMethodManager.SHOW_IMPLICIT)
        }
    }

    private fun hideIme() {
        keyboardShown = false
        inputMethodManager().hideSoftInputFromWindow(windowToken, 0)
    }

    private fun inputMethodManager(): InputMethodManager {
        return context.getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager
    }

    private fun handleScrollTouch(event: MotionEvent): Boolean {
        val action = event.actionMasked
        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            if (scrollGrab != 0) {
                scrollGrab = 0
                return true
            }
            return false
        }
        if ((scrollState and (SCROLL_ACTIVE or SCROLL_MAX)) == 0) {
            return false
        }
        letterbox(width, height, gameW, gameH, dstRect)
        layoutOverlayButtons()
        if (action == MotionEvent.ACTION_DOWN) {
            if ((scrollState and SCROLL_ACTIVE != 0) && upRect.contains(event.x, event.y)) {
                scrollGrab = 1
                parent.requestDisallowInterceptTouchEvent(true)
                GameNative.listScrollPage(-1)
                return true
            }
            if ((scrollState and SCROLL_ACTIVE != 0) && dnRect.contains(event.x, event.y)) {
                scrollGrab = 2
                parent.requestDisallowInterceptTouchEvent(true)
                GameNative.listScrollPage(1)
                return true
            }
            if ((scrollState and SCROLL_MAX != 0) && maxRect.contains(event.x, event.y)) {
                scrollGrab = 3
                parent.requestDisallowInterceptTouchEvent(true)
                GameNative.listJumpMax()
                return true
            }
            return false
        }
        return scrollGrab != 0
    }

    private fun layoutOverlayButtons() {
        val density = resources.displayMetrics.density
        val edge = 8f * density
        val rail = 56f * density
        val btnH = rail
        val src = overlayGameRects
        if (scrollState and SCROLL_MAX != 0) {
            placeGutterButton(maxRect, edge, edge + rail, gameYToView(midY(src, 9)), btnH)
        } else {
            maxRect.setEmpty()
        }
        if (scrollState and SCROLL_ACTIVE != 0) {
            val right = width - edge
            val left = right - rail
            placeGutterButton(upRect, left, right, gameYToView(midY(src, 1)), btnH)
            placeGutterButton(dnRect, left, right, gameYToView(midY(src, 5)), btnH)
        } else {
            upRect.setEmpty()
            dnRect.setEmpty()
        }
    }

    private fun midY(src: IntArray, start: Int): Float {
        if (src.size < start + 4) {
            return gameH / 2f
        }
        return (src[start + 1] + src[start + 3] + 1) / 2f
    }

    private fun gameYToView(gy: Float): Float {
        if (dstRect.height() <= 0 || gameH <= 0) {
            return height / 2f
        }
        return dstRect.top + gy * dstRect.height() / gameH
    }

    private fun placeGutterButton(out: RectF, left: Float, right: Float, centerY: Float, btnH: Float) {
        var top = centerY - btnH / 2f
        var bottom = centerY + btnH / 2f
        if (top < 8f * resources.displayMetrics.density) {
            bottom += 8f * resources.displayMetrics.density - top
            top = 8f * resources.displayMetrics.density
        }
        if (bottom > height - 8f * resources.displayMetrics.density) {
            top -= bottom - (height - 8f * resources.displayMetrics.density)
            bottom = height - 8f * resources.displayMetrics.density
        }
        out.set(left, top, right, bottom)
    }

    private fun drawLcarsButton(canvas: Canvas, rect: RectF, color: Int, enabled: Boolean) {
        overlayFill.color = if (enabled) color else lcarsGray
        val radius = 5f * resources.displayMetrics.density
        canvas.drawRoundRect(rect, radius, radius, overlayFill)
    }

    private fun drawChevron(canvas: Canvas, rect: RectF, up: Boolean, enabled: Boolean) {
        val cx = rect.centerX()
        val cy = rect.centerY()
        val w = rect.width() * 0.28f
        val h = rect.height() * 0.16f
        chevron.reset()
        if (up) {
            chevron.moveTo(cx, cy - h)
            chevron.lineTo(cx + w, cy + h * 0.7f)
            chevron.lineTo(cx - w, cy + h * 0.7f)
        } else {
            chevron.moveTo(cx, cy + h)
            chevron.lineTo(cx + w, cy - h * 0.7f)
            chevron.lineTo(cx - w, cy - h * 0.7f)
        }
        chevron.close()
        overlayFill.color = if (enabled) lcarsBlack else lcarsButter
        canvas.drawPath(chevron, overlayFill)
    }

    private fun drawMaxLabel(canvas: Canvas, rect: RectF) {
        overlayText.color = lcarsBlack
        overlayText.textSize = rect.height() * 0.32f
        val fm = overlayText.fontMetrics
        val ty = rect.centerY() - (fm.ascent + fm.descent) / 2f
        canvas.drawText("MAX", rect.centerX(), ty, overlayText)
    }

    private fun viewToGameX(x: Float): Int {
        letterbox(width, height, gameW, gameH, dstRect)
        if (dstRect.width() <= 0) {
            return 0
        }
        val v = ((x - dstRect.left) * gameW / dstRect.width()).toInt()
        return v.coerceIn(0, gameW - 1)
    }

    private fun viewToGameY(y: Float): Int {
        letterbox(width, height, gameW, gameH, dstRect)
        if (dstRect.height() <= 0) {
            return 0
        }
        val v = ((y - dstRect.top) * gameH / dstRect.height()).toInt()
        return v.coerceIn(0, gameH - 1)
    }

    private class GameInputConnection(target: View) : BaseInputConnection(target, true) {
        override fun commitText(text: CharSequence?, newCursorPosition: Int): Boolean {
            if (text != null) {
                for (ch in text) {
                    sendChar(ch)
                }
            }
            return true
        }

        override fun setComposingText(text: CharSequence?, newCursorPosition: Int): Boolean {
            return commitText(text, newCursorPosition)
        }

        override fun deleteSurroundingText(beforeLength: Int, afterLength: Int): Boolean {
            repeat(beforeLength.coerceAtLeast(0)) { sendKey(8, 8) }
            return true
        }

        override fun sendKeyEvent(event: KeyEvent): Boolean {
            if (event.action != KeyEvent.ACTION_DOWN) {
                return true
            }
            when (event.keyCode) {
                KeyEvent.KEYCODE_DEL -> sendKey(8, 8)
                KeyEvent.KEYCODE_ENTER, KeyEvent.KEYCODE_NUMPAD_ENTER -> sendKey(13, 13)
                KeyEvent.KEYCODE_ESCAPE -> sendKey(27, 27)
                else -> {
                    val ch = event.unicodeChar and 0xff
                    if (ch != 0) {
                        sendChar(ch.toChar())
                    }
                }
            }
            return true
        }

        override fun performEditorAction(actionCode: Int): Boolean {
            sendKey(13, 13)
            return true
        }

        private fun sendChar(ch: Char) {
            val c = ch.code and 0xff
            if (c == 0) {
                return
            }
            val key = if (c in 65..90) c + 32 else c
            sendKey(key, c)
        }

        private fun sendKey(key: Int, ch: Int) {
            GameNative.onKey(key, ch, true)
            GameNative.onKey(key, ch, false)
        }
    }

    companion object {
        private const val SCROLL_ACTIVE = 1
        private const val SCROLL_UP = 2
        private const val SCROLL_DOWN = 4
        private const val SCROLL_MAX = 8

        fun letterbox(vw: Int, vh: Int, gw: Int, gh: Int, out: Rect) {
            if (vw <= 0 || vh <= 0 || gw <= 0 || gh <= 0) {
                out.set(0, 0, 0, 0)
                return
            }
            val scale = min(vw.toFloat() / gw, vh.toFloat() / gh)
            val dw = (gw * scale).toInt()
            val dh = (gh * scale).toInt()
            val left = (vw - dw) / 2
            val top = (vh - dh) / 2
            out.set(left, top, left + dw, top + dh)
        }
    }
}
