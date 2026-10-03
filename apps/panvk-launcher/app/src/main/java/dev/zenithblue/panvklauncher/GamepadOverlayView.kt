// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.SparseArray
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import kotlin.math.hypot

/**
 * Translucent on-screen Xbox pad. Feeds [GamepadBridge] virtual inputs. Returns true from
 * onTouchEvent only when a gesture starts on a control, so a sibling view below still gets other touches.
 * Limit: a gesture that starts outside the controls never reaches this view, so a second finger of that
 * gesture cannot press a control (Android routes a gesture to one target).
 */
class GamepadOverlayView(context: Context) : View(context) {
    private enum class Kind { STICK, DPAD, BTN, TRIG, TOGGLE }

    private class Ctl(val kind: Kind, val arg: Int, var label: String, val round: Boolean, val color: Int) {
        val rect = RectF()
        var pid = -1
        var pressed = false
        var dx = 0f
        var dy = 0f
        var hat = 0
        val cx get() = rect.centerX()
        val cy get() = rect.centerY()
    }

    private val ctls = ArrayList<Ctl>()
    private val owner = SparseArray<Ctl>()
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = Color.WHITE }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE; textAlign = Paint.Align.CENTER }
    private var alphaF = 0.4f
    private val tmp = RectF()

    /** Hide the right stick (e.g. when the touchpad drives the camera). */
    var rightStickEnabled = true
        set(v) { if (field != v) { releaseAll(); field = v; buildLayout(width, height); invalidate() } }

    fun setOverlayAlpha(a: Float) { alphaF = a.coerceIn(0.1f, 1f); invalidate() }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) { buildLayout(w, h) }

    // Safe area (system bars + display cutout), px. Set from window insets.
    private var insL = 0f; private var insT = 0f; private var insR = 0f; private var insB = 0f

    override fun onApplyWindowInsets(insets: WindowInsets): WindowInsets {
        var l = insets.systemWindowInsetLeft; var t = insets.systemWindowInsetTop
        var r = insets.systemWindowInsetRight; var b = insets.systemWindowInsetBottom
        insets.displayCutout?.let {
            l = maxOf(l, it.safeInsetLeft); t = maxOf(t, it.safeInsetTop)
            r = maxOf(r, it.safeInsetRight); b = maxOf(b, it.safeInsetBottom)
        }
        insL = l.toFloat(); insT = t.toFloat(); insR = r.toFloat(); insB = b.toFloat()
        buildLayout(width, height)
        invalidate()
        return insets
    }

    /** Hide everything except the HIDE/SHOW button. */
    var hidden = false
        set(v) { releaseAll(); field = v; invalidate() }

    private var toggle: Ctl? = null
    private fun active(): List<Ctl> = if (hidden) listOfNotNull(toggle) else ctls

    // Sizes: dp base value * scale k, clamped to a dp minimum (>= 40dp touch targets), then converted to px
    // with the display density, so the pad follows UI density / "display size". The search below picks the
    // biggest k that fits without overlap; if even the minimum sizes do not fit it drops optional controls
    // (triggers, then right stick, then bumpers) and tries a side-by-side arrangement instead of overlapping.
    private var ax0 = 0f; private var ax1 = 0f; private var ay0 = 0f; private var ay1 = 0f
    private var shownScale = 1f

    private fun buildLayout(w: Int, h: Int) {
        releaseAll()
        ctls.clear()
        toggle = null
        if (w <= 0 || h <= 0) return
        var last: List<Ctl> = emptyList()
        search@ for (level in 0..3) {
            var k = 1.25f
            while (k >= 0.6f - 1e-3f) {
                for (stacked in booleanArrayOf(true, false)) for (sep in booleanArrayOf(false, true)) {
                    last = layout(w, h, level, k, stacked, sep)
                    if (fits(last)) break@search
                }
                k -= 0.05f
            }
        }
        ctls.addAll(last)
        toggle = ctls.firstOrNull { it.kind == Kind.TOGGLE }
    }

    private fun fits(l: List<Ctl>): Boolean {
        val g = 2 * resources.displayMetrics.density
        for (i in l.indices) {
            val a = l[i].rect
            if (a.left < ax0 - 0.5f || a.right > ax1 + 0.5f || a.top < ay0 - 0.5f || a.bottom > ay1 + 0.5f) return false
            for (j in i + 1 until l.size) {
                val c = l[i]; val d = l[j]
                val hit = if (c.round && d.round) hypot(c.cx - d.cx, c.cy - d.cy) < c.rect.width() / 2 + d.rect.width() / 2 + g
                    else RectF.intersects(RectF(c.rect).apply { inset(-g / 2, -g / 2) }, RectF(d.rect).apply { inset(-g / 2, -g / 2) })
                if (hit) return false
            }
        }
        return true
    }

    private fun layout(w: Int, h: Int, level: Int, k: Float, stacked: Boolean, sep: Boolean): List<Ctl> {
        val dp = resources.displayMetrics.density
        fun u(base: Float, min: Float) = maxOf(base * k, min) * dp
        val g = 6 * dp
        ax0 = insL + 4 * dp; ax1 = w - insR - 4 * dp; ay0 = insT + 4 * dp; ay1 = h - insB - 4 * dp
        val rsOn = rightStickEnabled && level < 2
        val R = u(52f, 36f); val dh = u(56f, 54f); val fr = u(24f, 20f); val fh = fr * 2.6f
        val shW = u(56f, 48f); val shH = u(40f, 40f); val cw = u(58f, 52f); val cH = u(36f, 40f)
        val rowH = maxOf(shH, cH)
        val cyC = ay0 + rowH / 2
        val shY = if (sep) ay0 + rowH + g + shH / 2 else cyC
        val out = ArrayList<Ctl>()
        fun add(c: Ctl, cx: Float, cy: Float, hw: Float, hh: Float = hw) { c.rect.set(cx - hw, cy - hh, cx + hw, cy + hh); out.add(c) }
        val sp = fr * 1.6f
        val lcx: Float; val rcx: Float; val stickY = ay1 - R
        val padX: Float; val padY: Float; val faceX: Float; val faceY: Float; val rsX: Float
        val shL: Float; val shR: Float
        if (stacked) {
            val colL = maxOf(2 * R, 2 * dh, 2 * shW + g); val colR = maxOf(if (rsOn) 2 * R else 0f, 2 * fh, 2 * shW + g)
            lcx = ax0 + colL / 2; rcx = ax1 - colR / 2
            padX = lcx; padY = stickY - R - g - dh
            faceX = rcx; faceY = if (rsOn) stickY - R - g - fh else ay1 - fh
            rsX = rcx
            shL = 0f; shR = 0f
        } else {
            lcx = ax0 + R; rcx = ax1 - R
            padX = ax0 + 2 * R + g + dh; padY = ay1 - dh
            rsX = rcx
            faceX = if (rsOn) ax1 - 2 * R - g - fh else ax1 - fh; faceY = ay1 - fh
            shL = ax0 + shW / 2; shR = ax1 - shW / 2
        }
        add(Ctl(Kind.STICK, 0, "", true, Color.WHITE), lcx, stickY, R)
        if (rsOn) add(Ctl(Kind.STICK, 1, "", true, Color.WHITE), rsX, stickY, R)
        add(Ctl(Kind.DPAD, 0, "", false, Color.WHITE), padX, padY, dh)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_A, "A", true, 0xFF3FB950.toInt()), faceX, faceY + sp, fr)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_B, "B", true, 0xFFE5534B.toInt()), faceX + sp, faceY, fr)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_X, "X", true, 0xFF4C8DF6.toInt()), faceX - sp, faceY, fr)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_Y, "Y", true, 0xFFE3B341.toInt()), faceX, faceY - sp, fr)
        // Shoulder rows: left = [LT][LB], right = [RB][RT], LB/RB toward the centre.
        val lStart = if (stacked) lcx - (shW + g) / 2 else shL          // centre of outer-left button
        val rEnd = if (stacked) rcx + (shW + g) / 2 else shR           // centre of outer-right button
        if (level < 3) {
            add(Ctl(Kind.BTN, GamepadBridge.BTN_LB, "LB", false, Color.WHITE), lStart + (if (level < 1) shW + g else 0f), shY, shW / 2, shH / 2)
            add(Ctl(Kind.BTN, GamepadBridge.BTN_RB, "RB", false, Color.WHITE), rEnd - (if (level < 1) shW + g else 0f), shY, shW / 2, shH / 2)
        }
        if (level < 1) {
            add(Ctl(Kind.TRIG, 0, "LT", false, Color.WHITE), lStart, shY, shW / 2, shH / 2)
            add(Ctl(Kind.TRIG, 1, "RT", false, Color.WHITE), rEnd, shY, shW / 2, shH / 2)
        }
        val cx = (ax0 + ax1) / 2
        add(Ctl(Kind.BTN, GamepadBridge.BTN_BACK, "BACK", false, Color.WHITE), cx - cw - g, cyC, cw / 2, cH / 2)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_START, "START", false, Color.WHITE), cx, cyC, cw / 2, cH / 2)
        add(Ctl(Kind.TOGGLE, 0, if (hidden) "SHOW" else "HIDE", false, Color.WHITE), cx + cw + g, cyC, cw / 2, cH / 2)
        line.strokeWidth = maxOf(1.5f, 1.5f * dp * k)
        text.textSize = 13f * dp * maxOf(k, 0.85f)
        shownScale = k
        return out
    }

    private fun hit(c: Ctl, x: Float, y: Float): Boolean =
        if (c.round) hypot(x - c.cx, y - c.cy) <= c.rect.width() / 2f else c.rect.contains(x, y)

    private fun apply(c: Ctl, x: Float, y: Float, down: Boolean) {
        when (c.kind) {
            Kind.STICK -> {
                val r = c.rect.width() / 2f
                var dx = x - c.cx; var dy = y - c.cy
                val len = hypot(dx, dy)
                if (len > r) { dx *= r / len; dy *= r / len }
                c.dx = dx / r; c.dy = dy / r
                GamepadBridge.setVirtualStick(c.arg == 0, c.dx, c.dy)
            }
            Kind.DPAD -> {
                val dx = x - c.cx; val dy = y - c.cy
                val len = hypot(dx, dy)
                var h = 0
                if (len > c.rect.width() * 0.12f) {
                    if (dx / len > 0.38f) h = h or GamepadBridge.HAT_RIGHT else if (dx / len < -0.38f) h = h or GamepadBridge.HAT_LEFT
                    if (dy / len > 0.38f) h = h or GamepadBridge.HAT_DOWN else if (dy / len < -0.38f) h = h or GamepadBridge.HAT_UP
                }
                c.hat = h; c.pressed = h != 0
                GamepadBridge.setVirtualHat(h)
            }
            Kind.BTN -> { c.pressed = down; GamepadBridge.setVirtualButton(c.arg, down) }
            Kind.TRIG -> { c.pressed = down; GamepadBridge.setVirtualTrigger(c.arg == 0, if (down) 1f else 0f) }
            Kind.TOGGLE -> { c.pressed = down; if (down) post { hidden = !hidden; c.label = if (hidden) "SHOW" else "HIDE" } }
        }
        if (c.kind == Kind.STICK) c.pressed = down
    }

    private fun release(c: Ctl) {
        c.pid = -1
        if (c.kind == Kind.STICK) { c.dx = 0f; c.dy = 0f; GamepadBridge.setVirtualStick(c.arg == 0, 0f, 0f); c.pressed = false }
        else if (c.kind == Kind.DPAD) { c.hat = 0; c.pressed = false; GamepadBridge.setVirtualHat(0) }
        else apply(c, 0f, 0f, false)
    }

    private fun releaseAll() { ctls.forEach { if (it.pid >= 0) release(it) }; owner.clear() }

    override fun onDetachedFromWindow() { releaseAll(); super.onDetachedFromWindow() }

    override fun onTouchEvent(e: MotionEvent): Boolean {
        when (e.actionMasked) {
            MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                val i = e.actionIndex
                val c = active().firstOrNull { it.pid < 0 && hit(it, e.getX(i), e.getY(i)) }
                if (c != null) {
                    c.pid = e.getPointerId(i); owner.put(c.pid, c)
                    apply(c, e.getX(i), e.getY(i), true); invalidate()
                    return true
                }
                return owner.size() > 0
            }
            MotionEvent.ACTION_MOVE -> {
                for (i in 0 until e.pointerCount) {
                    val c = owner.get(e.getPointerId(i)) ?: continue
                    if (c.kind == Kind.STICK || c.kind == Kind.DPAD) apply(c, e.getX(i), e.getY(i), true)
                }
                invalidate()
                return owner.size() > 0
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP -> {
                val pid = e.getPointerId(e.actionIndex)
                owner.get(pid)?.let { release(it); owner.remove(pid) }
                invalidate()
                return true
            }
            MotionEvent.ACTION_CANCEL -> { releaseAll(); invalidate(); return true }
        }
        return false
    }

    override fun onDraw(canvas: Canvas) {
        for (c in active()) {
            val a = alphaF * (if (c.pressed) 1.8f else 1f)
            fill.color = c.color; fill.alpha = (a * 160).toInt().coerceAtMost(255)
            line.alpha = (a * 255).toInt().coerceAtMost(255); text.alpha = line.alpha
            val r = c.rect
            when (c.kind) {
                Kind.STICK -> {
                    val rad = r.width() / 2f
                    canvas.drawCircle(c.cx, c.cy, rad, line)
                    canvas.drawCircle(c.cx + c.dx * rad, c.cy + c.dy * rad, rad * 0.45f, fill)
                }
                Kind.DPAD -> {
                    val h = r.width() / 2f; val w = h / 3f
                    arm(canvas, c, GamepadBridge.HAT_UP, c.cx - w, c.cy - h, c.cx + w, c.cy - w)
                    arm(canvas, c, GamepadBridge.HAT_DOWN, c.cx - w, c.cy + w, c.cx + w, c.cy + h)
                    arm(canvas, c, GamepadBridge.HAT_LEFT, c.cx - h, c.cy - w, c.cx - w, c.cy + w)
                    arm(canvas, c, GamepadBridge.HAT_RIGHT, c.cx + w, c.cy - w, c.cx + h, c.cy + w)
                }
                else -> {
                    if (c.round) { canvas.drawCircle(c.cx, c.cy, r.width() / 2f, fill); canvas.drawCircle(c.cx, c.cy, r.width() / 2f, line) }
                    else { val k = r.height() / 3f; canvas.drawRoundRect(r, k, k, fill); canvas.drawRoundRect(r, k, k, line) }
                    canvas.drawText(c.label, c.cx, c.cy + text.textSize * 0.35f, text)
                }
            }
        }
    }

    private fun arm(canvas: Canvas, c: Ctl, bit: Int, l: Float, t: Float, r: Float, b: Float) {
        val on = c.hat and bit != 0
        fill.alpha = ((alphaF * (if (on) 1.8f else 1f)) * 160).toInt().coerceAtMost(255)
        tmp.set(l, t, r, b)
        canvas.drawRoundRect(tmp, (r - l) / 4f, (r - l) / 4f, fill)
        canvas.drawRoundRect(tmp, (r - l) / 4f, (r - l) / 4f, line)
    }
}
