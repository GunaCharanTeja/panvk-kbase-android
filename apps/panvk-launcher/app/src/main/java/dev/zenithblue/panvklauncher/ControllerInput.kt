// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import com.winlator.xserver.Pointer
import com.winlator.xserver.XKeycode

/**
 * Turns the merged pad state (overlay + physical, from [GamepadBridge]) into X11 keyboard / pointer input on the
 * built-in X server, per [ControllerConfig]. Wine sees ordinary key and mouse events. Right stick (or left) in
 * "mouse" mode drives relative pointer motion from a 120 Hz ticker.
 */
object ControllerInput {
    @Volatile var config: ControllerConfig = ControllerConfig.FALLBACK
    val gamepadOutput get() = config.output != "keyboard"
    private val keyboardOutput get() = config.output != "gamepad"

    private const val PX_PER_SEC = 900f
    private val lock = Any()
    private val heldKeys = HashSet<XKeycode>()
    private val heldMouse = HashSet<Pointer.Button>()
    private val wheelHeld = HashSet<String>()
    @Volatile private var mvx = 0f
    @Volatile private var mvy = 0f
    @Volatile private var running = false
    private var ticker: Thread? = null

    /** Merged pad state. Sticks -1..1 (y down positive), triggers 0..1, btn = GamepadBridge.BTN_* bits, hat = HAT_* bits. */
    fun update(lx: Float, ly: Float, rx: Float, ry: Float, lt: Float, rt: Float, btn: Int, hat: Int) {
        if (!keyboardOutput) return
        val want = LinkedHashSet<String>()
        fun bit(b: Int, id: String) { if (btn and (1 shl b) != 0) want += id }
        bit(GamepadBridge.BTN_A, "A"); bit(GamepadBridge.BTN_B, "B"); bit(GamepadBridge.BTN_X, "X"); bit(GamepadBridge.BTN_Y, "Y")
        bit(GamepadBridge.BTN_LB, "LB"); bit(GamepadBridge.BTN_RB, "RB"); bit(GamepadBridge.BTN_BACK, "BACK")
        bit(GamepadBridge.BTN_START, "START"); bit(GamepadBridge.BTN_LSTICK, "L3"); bit(GamepadBridge.BTN_RSTICK, "R3")
        if (lt > 0.5f) want += "LT"
        if (rt > 0.5f) want += "RT"
        if (hat and GamepadBridge.HAT_UP != 0) want += "DPAD_UP"
        if (hat and GamepadBridge.HAT_DOWN != 0) want += "DPAD_DOWN"
        if (hat and GamepadBridge.HAT_LEFT != 0) want += "DPAD_LEFT"
        if (hat and GamepadBridge.HAT_RIGHT != 0) want += "DPAD_RIGHT"
        val c = config
        val targets = LinkedHashSet<String>()
        want.forEach { id -> c.bindings[id]?.let { targets += it } }
        var mx = 0f; var my = 0f
        for ((s, x, y) in listOf(Triple(c.leftStick, lx, ly), Triple(c.rightStick, rx, ry))) {
            if (s.mode == "none") continue
            if (s.mode == "mouse") {
                val m = hypotf(x, y)
                if (m > s.deadzone) {
                    val k = (m - s.deadzone) / (1f - s.deadzone) // 0..1 past the deadzone
                    val speed = Math.pow(k.toDouble(), 1.5).toFloat() * s.sensitivity // k^1.5 response curve
                    mx += x / m * speed; my += y / m * speed * (if (s.invertY) -1f else 1f)
                }
            } else {
                if (y < -s.deadzone) targets += s.up
                if (y > s.deadzone) targets += s.down
                if (x < -s.deadzone) targets += s.left
                if (x > s.deadzone) targets += s.right
            }
        }
        mvx = mx; mvy = my
        apply(targets)
    }

    private fun hypotf(x: Float, y: Float) = Math.sqrt((x * x + y * y).toDouble()).toFloat()

    private fun apply(targets: Set<String>): Unit = synchronized(lock) {
        val xs = BuiltinXServer.xServer ?: return@synchronized
        val keys = HashSet<XKeycode>(); val mouse = HashSet<Pointer.Button>(); val wheels = HashSet<String>()
        for (t in targets) {
            val m = ControllerKeys.mouse(t)
            when {
                m == Pointer.Button.BUTTON_SCROLL_UP || m == Pointer.Button.BUTTON_SCROLL_DOWN -> wheels += t
                m != null -> mouse += m
                else -> ControllerKeys.key(t)?.let { keys += it }
            }
        }
        for (k in heldKeys.toList()) if (k !in keys) { xs.injectKeyRelease(k); heldKeys -= k }
        for (k in keys) if (heldKeys.add(k)) xs.injectKeyPress(k)
        for (b in heldMouse.toList()) if (b !in mouse) { xs.injectPointerButtonRelease(b); heldMouse -= b }
        for (b in mouse) if (heldMouse.add(b)) xs.injectPointerButtonPress(b)
        for (w in wheels) if (wheelHeld.add(w)) {
            val b = ControllerKeys.mouse(w)!!
            xs.injectPointerButtonPress(b); xs.injectPointerButtonRelease(b)
        }
        wheelHeld.retainAll(wheels)
    }

    /** Called when the X server starts. */
    fun start() {
        if (running) return
        running = true
        ticker = Thread({
            var accX = 0f; var accY = 0f
            var last = System.nanoTime()
            while (running) {
                try { Thread.sleep(8) } catch (_: InterruptedException) { break }
                val now = System.nanoTime(); val dt = (now - last) / 1e9f; last = now
                val vx = mvx; val vy = mvy
                if (vx == 0f && vy == 0f) { accX = 0f; accY = 0f; continue }
                accX += vx * PX_PER_SEC * dt; accY += vy * PX_PER_SEC * dt
                val dx = accX.toInt(); val dy = accY.toInt()
                if (dx != 0 || dy != 0) {
                    accX -= dx; accY -= dy
                    BuiltinXServer.xServer?.injectPointerMoveDelta(dx, dy)
                }
            }
        }, "pad-mouse").apply { isDaemon = true; start() }
    }

    /** Called when the X server stops: release everything. */
    fun stop() {
        running = false
        ticker?.interrupt(); ticker = null
        mvx = 0f; mvy = 0f
        synchronized(lock) {
            val xs = BuiltinXServer.xServer
            if (xs != null) {
                heldKeys.forEach { xs.injectKeyRelease(it) }
                heldMouse.forEach { xs.injectPointerButtonRelease(it) }
            }
            heldKeys.clear(); heldMouse.clear(); wheelHeld.clear()
        }
    }
}
