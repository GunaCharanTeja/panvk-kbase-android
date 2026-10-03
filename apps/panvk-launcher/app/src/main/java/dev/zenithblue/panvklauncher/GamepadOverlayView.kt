// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Typeface
import android.os.Build
import android.os.SystemClock
import android.os.VibrationEffect
import android.os.Vibrator
import android.util.SparseArray
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.View
import android.view.WindowInsets
import kotlin.math.atan2
import kotlin.math.hypot
import kotlin.math.roundToInt

/** Overlay look + feel, "launcher" prefs. */
object OverlayPrefs {
    private fun p(c: Context) = c.getSharedPreferences("launcher", Context.MODE_PRIVATE)
    fun opacity(c: Context) = p(c).getFloat("overlay_opacity", 0.7f)
    fun setOpacity(c: Context, v: Float) = p(c).edit().putFloat("overlay_opacity", v.coerceIn(0.2f, 1f)).apply()
    fun size(c: Context) = p(c).getFloat("overlay_size", 1f)
    fun setSize(c: Context, v: Float) = p(c).edit().putFloat("overlay_size", v.coerceIn(0.7f, 1.4f)).apply()
    /** off | light | strong */
    fun haptics(c: Context) = p(c).getString("overlay_haptics", "light") ?: "light"
    fun setHaptics(c: Context, v: String) = p(c).edit().putString("overlay_haptics", v).apply()
}

/**
 * Translucent on-screen Xbox pad. Feeds [GamepadBridge] virtual inputs. Returns true from
 * onTouchEvent only when a gesture starts on a control, so a sibling view below still gets other touches.
 * Limit: a gesture that starts outside the controls never reaches this view, so a second finger of that
 * gesture cannot press a control (Android routes a gesture to one target).
 *
 * Preview mode ([preview] set): draws [preview]'s captions laid out for a virtual screen of [previewW] x [previewH]
 * px scaled into the view, sends nothing to the pad, and reports taps as control ids via [onPreviewTap].
 */
class GamepadOverlayView(context: Context) : View(context) {
    private enum class Kind { STICK, DPAD, BTN, TRIG, TOGGLE, EXIT }

    private class Ctl(val kind: Kind, val arg: Int, var label: String, val round: Boolean, val color: Int) {
        val rect = RectF()
        var pid = -1
        var pressed = false
        var anim = 0f // 0 = up, 1 = fully pressed (animated)
        var dx = 0f
        var dy = 0f
        var hat = 0
        var dir = -1 // stick octant past the haptic deadzone, -1 = centred
        val cx get() = rect.centerX()
        val cy get() = rect.centerY()
    }

    private val ctls = ArrayList<Ctl>()
    private val owner = SparseArray<Ctl>()
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG)
    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.STROKE; color = Color.WHITE }
    private val text = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE; textAlign = Paint.Align.CENTER; typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)
    }
    private var opacity = OverlayPrefs.opacity(context)
    private var userScale = OverlayPrefs.size(context)
    private var hapticLevel = OverlayPrefs.haptics(context)
    private val tmp = RectF()
    private val path = Path()

    var preview: ControllerConfig? = null
        set(v) { field = v; invalidate() }
    var previewW = 0
    var previewH = 0
    var onPreviewTap: ((String) -> Unit)? = null
    private val previewing get() = preview != null && previewW > 0 && previewH > 0
    private val cfg get() = preview ?: ControllerInput.config

    /** Re-read opacity / size / haptics prefs (settings changed). */
    fun reloadPrefs() {
        opacity = OverlayPrefs.opacity(context); userScale = OverlayPrefs.size(context); hapticLevel = OverlayPrefs.haptics(context)
        rebuild(); invalidate()
    }

    /** Hide the right stick (e.g. when the touchpad drives the camera). */
    var rightStickEnabled = true
        set(v) { if (field != v) { releaseAll(); field = v; rebuild(); invalidate() } }

    fun setOverlayAlpha(a: Float) { opacity = a.coerceIn(0.1f, 1f); invalidate() }

    override fun onSizeChanged(w: Int, h: Int, oldw: Int, oldh: Int) { rebuild() }

    private fun rebuild() = if (previewing) buildLayout(previewW, previewH) else buildLayout(width, height)

    // Safe area (system bars + display cutout), px. Set from window insets.
    private var insL = 0f; private var insT = 0f; private var insR = 0f; private var insB = 0f

    override fun onApplyWindowInsets(insets: WindowInsets): WindowInsets {
        if (previewing) return insets
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
    private var exitCtl: Ctl? = null
    private fun active(): List<Ctl> = if (hidden) listOfNotNull(toggle, exitCtl) else ctls

    /** Called after EXIT was held for [EXIT_HOLD_MS]. */
    var onExit: (() -> Unit)? = null
    private val exitRun = Runnable {
        exitCtl?.let { c -> owner.remove(c.pid); release(c) }
        haptic(H_CONFIRM)
        invalidate(); onExit?.invoke()
    }
    private var exitDownAt = 0L
    private val EXIT_HOLD_MS = 1500L

    // ---- haptics: press = click, release = light tick, stick/dpad direction change = tick, EXIT = confirm ----
    private val H_PRESS = 0; private val H_RELEASE = 1; private val H_TICK = 2; private val H_CONFIRM = 3
    private var lastTickAt = 0L
    private val vibrator: Vibrator? by lazy { context.getSystemService(Vibrator::class.java) }

    private fun haptic(kind: Int) {
        if (previewing || hapticLevel == "off") return
        if (kind == H_TICK) { // no spam while a stick wobbles across a border
            val now = SystemClock.uptimeMillis()
            if (now - lastTickAt < 40) return
            lastTickAt = now
        }
        if (hapticLevel == "strong" && Build.VERSION.SDK_INT >= 29) {
            val v = vibrator ?: return
            if (!v.hasVibrator()) return
            val e = when (kind) {
                H_PRESS -> VibrationEffect.EFFECT_HEAVY_CLICK
                H_CONFIRM -> VibrationEffect.EFFECT_DOUBLE_CLICK
                H_TICK -> VibrationEffect.EFFECT_CLICK
                else -> VibrationEffect.EFFECT_TICK
            }
            try { v.vibrate(VibrationEffect.createPredefined(e)) } catch (_: SecurityException) {}
            return
        }
        val c = when (kind) {
            H_PRESS -> HapticFeedbackConstants.VIRTUAL_KEY
            H_RELEASE -> if (Build.VERSION.SDK_INT >= 27) HapticFeedbackConstants.VIRTUAL_KEY_RELEASE else return
            H_TICK -> HapticFeedbackConstants.CLOCK_TICK
            else -> if (Build.VERSION.SDK_INT >= 30) HapticFeedbackConstants.CONFIRM else HapticFeedbackConstants.LONG_PRESS
        }
        performHapticFeedback(c)
    }

    // Sizes: dp base value * scale k, clamped to a dp minimum (>= 40dp touch targets), then converted to px
    // with the display density, so the pad follows UI density / "display size". The search below picks the
    // biggest k (starting at the user's size setting) that fits without overlap; if even the minimum sizes do
    // not fit it drops optional controls (triggers, then right stick, then bumpers) and tries a side-by-side
    // arrangement instead of overlapping.
    private var ax0 = 0f; private var ax1 = 0f; private var ay0 = 0f; private var ay1 = 0f

    private fun buildLayout(w: Int, h: Int) {
        releaseAll()
        ctls.clear()
        toggle = null
        if (w <= 0 || h <= 0) return
        var last: List<Ctl> = emptyList()
        search@ for (level in 0..3) {
            var k = 1.25f * userScale
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
        exitCtl = ctls.firstOrNull { it.kind == Kind.EXIT }
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
        val il = if (previewing) 0f else insL; val itop = if (previewing) 0f else insT
        val ir = if (previewing) 0f else insR; val ib = if (previewing) 0f else insB
        ax0 = il + 4 * dp; ax1 = w - ir - 4 * dp; ay0 = itop + 4 * dp; ay1 = h - ib - 4 * dp
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
        add(Ctl(Kind.BTN, GamepadBridge.BTN_A, "A", true, 0xFF4ADE80.toInt()), faceX, faceY + sp, fr)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_B, "B", true, 0xFFF87171.toInt()), faceX + sp, faceY, fr)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_X, "X", true, 0xFF60A5FA.toInt()), faceX - sp, faceY, fr)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_Y, "Y", true, 0xFFFACC15.toInt()), faceX, faceY - sp, fr)
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
        // Top row: BACK START HIDE EXIT (EXIT = hold to confirm, red).
        val x0 = (ax0 + ax1) / 2 - (4 * cw + 3 * g) / 2 + cw / 2
        add(Ctl(Kind.BTN, GamepadBridge.BTN_BACK, "BACK", false, Color.WHITE), x0, cyC, cw / 2, cH / 2)
        add(Ctl(Kind.BTN, GamepadBridge.BTN_START, "START", false, Color.WHITE), x0 + (cw + g), cyC, cw / 2, cH / 2)
        add(Ctl(Kind.TOGGLE, 0, if (hidden) "SHOW" else "HIDE", false, Color.WHITE), x0 + 2 * (cw + g), cyC, cw / 2, cH / 2)
        add(Ctl(Kind.EXIT, 0, "EXIT", false, 0xFFF87171.toInt()), x0 + 3 * (cw + g), cyC, cw / 2, cH / 2)
        line.strokeWidth = maxOf(1.5f, 1.5f * dp * k)
        text.textSize = 13f * dp * maxOf(k, 0.85f)
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
                // Haptic only when the stick leaves the centre or changes octant, never per move event.
                val m = hypot(c.dx, c.dy)
                val dir = if (m < 0.35f) -1 else ((atan2(c.dy, c.dx) / (Math.PI / 4)).roundToInt() + 8) % 8
                if (dir != c.dir) { if (dir >= 0) haptic(H_TICK); c.dir = dir }
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
                if (h != c.hat && h != 0) haptic(if (c.hat == 0) H_PRESS else H_TICK)
                c.hat = h; c.pressed = h != 0
                GamepadBridge.setVirtualHat(h)
            }
            Kind.BTN -> { press(c, down); GamepadBridge.setVirtualButton(c.arg, down) }
            Kind.TRIG -> { press(c, down); GamepadBridge.setVirtualTrigger(c.arg == 0, if (down) 1f else 0f) }
            Kind.TOGGLE -> { press(c, down); if (down) post { hidden = !hidden; c.label = if (hidden) "SHOW" else "HIDE" } }
            Kind.EXIT -> {
                press(c, down)
                removeCallbacks(exitRun)
                if (down) { exitDownAt = SystemClock.uptimeMillis(); postDelayed(exitRun, EXIT_HOLD_MS); postInvalidateOnAnimation() }
            }
        }
        if (c.kind == Kind.STICK) c.pressed = down
    }

    private fun press(c: Ctl, down: Boolean) {
        if (down != c.pressed) haptic(if (down) H_PRESS else H_RELEASE)
        c.pressed = down
    }

    private fun release(c: Ctl) {
        c.pid = -1
        if (c.kind == Kind.STICK) { c.dx = 0f; c.dy = 0f; c.dir = -1; GamepadBridge.setVirtualStick(c.arg == 0, 0f, 0f); c.pressed = false }
        else if (c.kind == Kind.DPAD) { c.hat = 0; c.pressed = false; GamepadBridge.setVirtualHat(0) }
        else apply(c, 0f, 0f, false)
    }

    private fun releaseAll() { ctls.forEach { if (it.pid >= 0) release(it) }; owner.clear() }

    override fun onDetachedFromWindow() { releaseAll(); super.onDetachedFromWindow() }

    /** Config id a preview tap selects: A/B/..., DPAD_UP..., LS / RS for sticks. */
    private fun idAt(c: Ctl, x: Float, y: Float): String? = when (c.kind) {
        Kind.BTN -> btnIds.getOrNull(c.arg)?.takeIf { it.isNotEmpty() }
        Kind.TRIG -> if (c.arg == 0) "LT" else "RT"
        Kind.STICK -> if (c.arg == 0) "LS" else "RS"
        Kind.DPAD -> {
            val dx = x - c.cx; val dy = y - c.cy
            if (Math.abs(dx) > Math.abs(dy)) (if (dx > 0) "DPAD_RIGHT" else "DPAD_LEFT") else (if (dy > 0) "DPAD_DOWN" else "DPAD_UP")
        }
        else -> null
    }

    private val previewScale get() = if (previewing) minOf(width / previewW.toFloat(), height / previewH.toFloat()) else 1f

    override fun onTouchEvent(e: MotionEvent): Boolean {
        if (previewing) {
            if (e.actionMasked == MotionEvent.ACTION_UP) {
                val s = previewScale; val x = e.x / s; val y = e.y / s
                ctls.firstOrNull { hit(it, x, y) }?.let { c -> idAt(c, x, y)?.let { id -> performClick(); onPreviewTap?.invoke(id) } }
            }
            return true
        }
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

    override fun performClick(): Boolean { super.performClick(); return true }

    private var lastFrame = 0L

    override fun onDraw(canvas: Canvas) {
        val now = SystemClock.uptimeMillis()
        val step = if (lastFrame == 0L) 1f else ((now - lastFrame) / 70f).coerceIn(0f, 1f) // ~70 ms press animation
        lastFrame = now
        var animating = false
        if (previewing) { canvas.save(); val s = previewScale; canvas.scale(s, s) }
        val o = opacity
        for (c in active()) {
            val target = if (c.pressed) 1f else 0f
            c.anim += (target - c.anim) * step
            if (Math.abs(target - c.anim) > 0.01f) animating = true else c.anim = target
            val a = c.anim
            val r = c.rect
            line.color = c.color; line.alpha = (o * 235).toInt()
            text.color = c.color; text.alpha = (o * 255).toInt()
            when (c.kind) {
                Kind.STICK -> {
                    val rad = r.width() / 2f
                    fill.color = Color.BLACK; fill.alpha = (o * 90).toInt()
                    canvas.drawCircle(c.cx, c.cy, rad, fill)
                    line.alpha = (o * 140).toInt(); canvas.drawCircle(c.cx, c.cy, rad, line)
                    fill.color = Color.WHITE; fill.alpha = (o * (110 + 90 * a)).toInt().coerceAtMost(255)
                    canvas.drawCircle(c.cx + c.dx * rad, c.cy + c.dy * rad, rad * (0.42f + 0.04f * a), fill)
                    if (cfg.output != "gamepad") {
                        val cap = cfg.stickCaption(c.arg == 0)
                        text.color = Color.WHITE; text.alpha = (o * 200).toInt()
                        if (cap.isNotEmpty()) drawFit(canvas, cap, c.cx, c.cy - rad * 0.66f, rad * 1.1f, 0.8f)
                    }
                }
                Kind.DPAD -> {
                    val h = r.width() / 2f; val w = h / 3f
                    arm(canvas, c, GamepadBridge.HAT_UP, c.cx - w, c.cy - h, c.cx + w, c.cy - w, 0)
                    arm(canvas, c, GamepadBridge.HAT_DOWN, c.cx - w, c.cy + w, c.cx + w, c.cy + h, 2)
                    arm(canvas, c, GamepadBridge.HAT_LEFT, c.cx - h, c.cy - w, c.cx - w, c.cy + w, 3)
                    arm(canvas, c, GamepadBridge.HAT_RIGHT, c.cx + w, c.cy - w, c.cx + h, c.cy + w, 1)
                    fill.color = Color.BLACK; fill.alpha = (o * 90).toInt()
                    tmp.set(c.cx - w, c.cy - w, c.cx + w, c.cy + w); canvas.drawRect(tmp, fill)
                }
                else -> {
                    canvas.save()
                    val sc = 1f - 0.08f * a // pressed: shrink a little + fill with the accent colour
                    canvas.scale(sc, sc, c.cx, c.cy)
                    fill.color = Color.BLACK; fill.alpha = (o * 100).toInt()
                    if (c.round) canvas.drawCircle(c.cx, c.cy, r.width() / 2f, fill) else canvas.drawRoundRect(r, r.height() / 2f, r.height() / 2f, fill)
                    if (a > 0f) {
                        fill.color = c.color; fill.alpha = (o * 170 * a).toInt()
                        if (c.round) canvas.drawCircle(c.cx, c.cy, r.width() / 2f, fill) else canvas.drawRoundRect(r, r.height() / 2f, r.height() / 2f, fill)
                    }
                    if (c.kind == Kind.EXIT && c.pressed) { // hold progress
                        val p = ((SystemClock.uptimeMillis() - exitDownAt) / EXIT_HOLD_MS.toFloat()).coerceIn(0f, 1f)
                        canvas.save(); canvas.clipRect(r.left, r.top, r.left + r.width() * p, r.bottom)
                        fill.color = c.color; fill.alpha = 255; canvas.drawRoundRect(r, r.height() / 2f, r.height() / 2f, fill); canvas.restore()
                        animating = true
                    }
                    if (c.round) canvas.drawCircle(c.cx, c.cy, r.width() / 2f, line) else canvas.drawRoundRect(r, r.height() / 2f, r.height() / 2f, line)
                    if (a > 0.5f && c.color != Color.WHITE) { text.color = Color.WHITE; text.alpha = (o * 255).toInt() }
                    drawFit(canvas, if (c.kind == Kind.EXIT && c.pressed) "HOLD" else caption(c), c.cx, c.cy, r.width() * 0.84f, 1f)
                    canvas.restore()
                }
            }
        }
        if (previewing) canvas.restore()
        if (animating) postInvalidateOnAnimation() else lastFrame = 0L
    }

    private val btnIds = arrayOf("A", "B", "X", "Y", "BACK", "", "START", "L3", "R3", "LB", "RB")

    // Overlay caption from the controller config (label, else the bound key); stock pad names in gamepad output mode.
    private fun caption(c: Ctl): String {
        if (cfg.output == "gamepad") return c.label
        val id = when (c.kind) {
            Kind.BTN -> btnIds.getOrNull(c.arg)
            Kind.TRIG -> if (c.arg == 0) "LT" else "RT"
            else -> null
        }
        return if (id.isNullOrEmpty()) c.label else cfg.caption(id).ifEmpty { c.label }
    }

    /** Text centred at (x, y), shrunk to fit maxW. */
    private fun drawFit(canvas: Canvas, s: String, x: Float, y: Float, maxW: Float, sizeK: Float) {
        val saved = text.textSize
        text.textSize = saved * sizeK
        val w = text.measureText(s)
        if (w > maxW) text.textSize = text.textSize * maxW / w
        canvas.drawText(s, x, y + text.textSize * 0.35f, text)
        text.textSize = saved
    }

    /** One D-pad arm with a small arrow glyph pointing outwards (dir 0 up, 1 right, 2 down, 3 left). */
    private fun arm(canvas: Canvas, c: Ctl, bit: Int, l: Float, t: Float, r: Float, b: Float, dir: Int) {
        val on = c.hat and bit != 0
        val o = opacity
        tmp.set(l, t, r, b)
        val rr = minOf(r - l, b - t) / 3f
        fill.color = Color.BLACK; fill.alpha = (o * 90).toInt()
        canvas.drawRoundRect(tmp, rr, rr, fill)
        if (on) { fill.color = Color.WHITE; fill.alpha = (o * 150).toInt(); canvas.drawRoundRect(tmp, rr, rr, fill) }
        line.color = Color.WHITE; line.alpha = (o * 140).toInt()
        canvas.drawRoundRect(tmp, rr, rr, line)
        val cx = (l + r) / 2; val cy = (t + b) / 2; val s = minOf(r - l, b - t) * 0.2f
        path.reset()
        when (dir) {
            0 -> { path.moveTo(cx, cy - s); path.lineTo(cx - s, cy + s * 0.6f); path.lineTo(cx + s, cy + s * 0.6f) }
            2 -> { path.moveTo(cx, cy + s); path.lineTo(cx - s, cy - s * 0.6f); path.lineTo(cx + s, cy - s * 0.6f) }
            3 -> { path.moveTo(cx - s, cy); path.lineTo(cx + s * 0.6f, cy - s); path.lineTo(cx + s * 0.6f, cy + s) }
            else -> { path.moveTo(cx + s, cy); path.lineTo(cx - s * 0.6f, cy - s); path.lineTo(cx - s * 0.6f, cy + s) }
        }
        path.close()
        fill.color = if (on) Color.BLACK else Color.WHITE; fill.alpha = (o * 220).toInt()
        canvas.drawPath(path, fill)
    }
}
