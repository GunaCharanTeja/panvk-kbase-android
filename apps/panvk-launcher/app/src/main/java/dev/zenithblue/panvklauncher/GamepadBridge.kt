// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.content.Context
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import java.io.File
import java.io.RandomAccessFile
import java.nio.ByteOrder
import java.nio.MappedByteBuffer
import java.nio.channels.FileChannel
import kotlin.math.abs
import kotlin.math.max

/**
 * Android gamepad state -> 64-byte shared file -> libpadshim.so (LD_PRELOAD in Wine) -> SDL virtual
 * Xbox 360 pad -> winebus -> XInput. Layout (little endian) must match cpp/padshim.c:
 *  0 u32 seq | 4 u32 connected | 8 i16 lx,ly,rx,ry (down/right positive) | 16 i16 lt,rt (0..32767)
 * 20 u16 buttons (bit = BTN_*) | 22 u8 hat (HAT_*) | 24 u16 rumble_low | 26 u16 rumble_high | 28 u32 rumble_seq
 * Env for the Wine launch: [env]. The shim polls every 4 ms, no JNI.
 */
object GamepadBridge {
    const val BTN_A = 0; const val BTN_B = 1; const val BTN_X = 2; const val BTN_Y = 3
    const val BTN_BACK = 4; const val BTN_GUIDE = 5; const val BTN_START = 6
    const val BTN_LSTICK = 7; const val BTN_RSTICK = 8; const val BTN_LB = 9; const val BTN_RB = 10
    const val HAT_UP = 1; const val HAT_RIGHT = 2; const val HAT_DOWN = 4; const val HAT_LEFT = 8
    private const val DEADZONE = 0.1f

    private val lock = Any()
    private var buf: java.nio.ByteBuffer? = null
    private var connected = 0

    // Physical (hardware pad) and virtual (on-screen overlay) state.
    private var pBtn = 0; private var pKeyHat = 0; private var pAxisHat = 0
    private var pStick = FloatArray(4); private var pLtAxis = 0f; private var pRtAxis = 0f
    private var pLtKey = false; private var pRtKey = false
    private var vBtn = 0; private var vHat = 0
    private var vStick = FloatArray(4); private var vLt = 0f; private var vRt = 0f

    private fun map(ctx: Context): File {
        val f = File(File(ctx.filesDir, "gamepad_shm").apply { mkdirs() }, "gamepad.mem")
        synchronized(lock) {
            if (buf == null) {
                RandomAccessFile(f, "rw").use { raf ->
                    raf.setLength(64)
                    raf.write(ByteArray(64)) // zeroed (also resets seq/connected from a previous run)
                    buf = raf.channel.map(FileChannel.MapMode.READ_WRITE, 0, 64).order(ByteOrder.LITTLE_ENDIAN)
                }
            }
        }
        return f
    }

    /** Env vars to add to the Wine process environment; empty if libpadshim.so is not installed. */
    fun env(ctx: Context): Map<String, String> {
        if (!ControllerInput.gamepadOutput) return emptyMap() // keyboard-only: no virtual XInput pad for the game to see
        val so = File(ctx.applicationInfo.nativeLibraryDir, "libpadshim.so")
        if (!so.exists()) return emptyMap()
        return mapOf("LD_PRELOAD" to so.path, "PANVK_PAD_SHM" to map(ctx).path)
    }

    fun start(ctx: Context) {
        if (ControllerInput.gamepadOutput) map(ctx)
        synchronized(lock) { connected = if (ControllerInput.gamepadOutput) 1 else 0; flush() }
    }

    fun stop() = synchronized(lock) {
        pBtn = 0; pKeyHat = 0; pAxisHat = 0; pStick = FloatArray(4); pLtAxis = 0f; pRtAxis = 0f
        pLtKey = false; pRtKey = false
        vBtn = 0; vHat = 0; vStick = FloatArray(4); vLt = 0f; vRt = 0f
        connected = 0; flush()
    }

    fun physicalConnected(): Boolean = InputDevice.getDeviceIds().any { id ->
        InputDevice.getDevice(id)?.let { !it.isVirtual && isPad(it.sources) } == true
    }

    /** Low 16 bits = low-frequency rumble, high 16 = high-frequency (0..65535), as requested by Wine. */
    fun rumble(): Int = synchronized(lock) { buf?.getInt(24) ?: 0 }
    fun rumbleSeq(): Int = synchronized(lock) { buf?.getInt(28) ?: 0 }

    // ---- physical input ----
    private fun isPad(src: Int) = (src and InputDevice.SOURCE_GAMEPAD) == InputDevice.SOURCE_GAMEPAD ||
        (src and InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK

    fun onKeyEvent(e: KeyEvent): Boolean {
        if (!isPad(e.source)) return false
        val down = e.action == KeyEvent.ACTION_DOWN
        if (!down && e.action != KeyEvent.ACTION_UP) return false
        val btn = when (e.keyCode) {
            KeyEvent.KEYCODE_BUTTON_A -> BTN_A
            KeyEvent.KEYCODE_BUTTON_B -> BTN_B
            KeyEvent.KEYCODE_BUTTON_X -> BTN_X
            KeyEvent.KEYCODE_BUTTON_Y -> BTN_Y
            KeyEvent.KEYCODE_BUTTON_L1 -> BTN_LB
            KeyEvent.KEYCODE_BUTTON_R1 -> BTN_RB
            KeyEvent.KEYCODE_BUTTON_THUMBL -> BTN_LSTICK
            KeyEvent.KEYCODE_BUTTON_THUMBR -> BTN_RSTICK
            KeyEvent.KEYCODE_BUTTON_START -> BTN_START
            KeyEvent.KEYCODE_BUTTON_SELECT -> BTN_BACK
            KeyEvent.KEYCODE_BUTTON_MODE -> BTN_GUIDE
            else -> -1
        }
        val hat = when (e.keyCode) {
            KeyEvent.KEYCODE_DPAD_UP -> HAT_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> HAT_DOWN
            KeyEvent.KEYCODE_DPAD_LEFT -> HAT_LEFT
            KeyEvent.KEYCODE_DPAD_RIGHT -> HAT_RIGHT
            else -> 0
        }
        synchronized(lock) {
            when {
                btn >= 0 -> pBtn = if (down) pBtn or (1 shl btn) else pBtn and (1 shl btn).inv()
                hat != 0 -> pKeyHat = if (down) pKeyHat or hat else pKeyHat and hat.inv()
                e.keyCode == KeyEvent.KEYCODE_BUTTON_L2 -> pLtKey = down
                e.keyCode == KeyEvent.KEYCODE_BUTTON_R2 -> pRtKey = down
                else -> return false
            }
            flush()
        }
        return true
    }

    fun onMotionEvent(e: MotionEvent): Boolean {
        if ((e.source and InputDevice.SOURCE_JOYSTICK) != InputDevice.SOURCE_JOYSTICK ||
            e.action != MotionEvent.ACTION_MOVE) return false
        val d = e.device
        fun has(axis: Int) = d?.getMotionRange(axis, e.source) != null
        val (rxA, ryA) = if (has(MotionEvent.AXIS_Z) && has(MotionEvent.AXIS_RZ)) MotionEvent.AXIS_Z to MotionEvent.AXIS_RZ
            else MotionEvent.AXIS_RX to MotionEvent.AXIS_RY
        val (ltA, rtA) = if (has(MotionEvent.AXIS_LTRIGGER) || has(MotionEvent.AXIS_RTRIGGER)) MotionEvent.AXIS_LTRIGGER to MotionEvent.AXIS_RTRIGGER
            else MotionEvent.AXIS_BRAKE to MotionEvent.AXIS_GAS
        val hx = e.getAxisValue(MotionEvent.AXIS_HAT_X); val hy = e.getAxisValue(MotionEvent.AXIS_HAT_Y)
        synchronized(lock) {
            pStick = floatArrayOf(e.getAxisValue(MotionEvent.AXIS_X), e.getAxisValue(MotionEvent.AXIS_Y),
                e.getAxisValue(rxA), e.getAxisValue(ryA))
            pLtAxis = e.getAxisValue(ltA); pRtAxis = e.getAxisValue(rtA)
            pAxisHat = (if (hx < -0.5f) HAT_LEFT else if (hx > 0.5f) HAT_RIGHT else 0) or
                (if (hy < -0.5f) HAT_UP else if (hy > 0.5f) HAT_DOWN else 0)
            flush()
        }
        return true
    }

    // ---- virtual (overlay) input ----
    fun setVirtualButton(bit: Int, down: Boolean) = synchronized(lock) {
        vBtn = if (down) vBtn or (1 shl bit) else vBtn and (1 shl bit).inv(); flush()
    }

    /** x right positive, y down positive, -1..1. */
    fun setVirtualStick(left: Boolean, x: Float, y: Float) = synchronized(lock) {
        val o = if (left) 0 else 2
        vStick[o] = x; vStick[o + 1] = y; flush()
    }

    fun setVirtualTrigger(left: Boolean, v: Float) = synchronized(lock) {
        if (left) vLt = v else vRt = v; flush()
    }

    fun setVirtualHat(mask: Int) = synchronized(lock) { vHat = mask and 15; flush() }

    // ---- merge + write (lock held) ----
    private fun s16(v: Float) = (v.coerceIn(-1f, 1f) * 32767f).toInt().toShort()

    /** Per-stick: whichever source deflects further wins; physical gets a deadzone. */
    private fun stick(o: Int): Pair<Float, Float> {
        var px = pStick[o]; var py = pStick[o + 1]
        if (px * px + py * py < DEADZONE * DEADZONE) { px = 0f; py = 0f }
        val vx = vStick[o]; val vy = vStick[o + 1]
        return if (vx * vx + vy * vy > px * px + py * py) vx to vy else px to py
    }

    private fun trig(axis: Float, key: Boolean, virt: Float): Float {
        val p = if (axis < DEADZONE) 0f else axis
        return max(max(p, if (key) 1f else 0f), abs(virt))
    }

    private fun flush() {
        val (lx, ly) = stick(0); val (rx, ry) = stick(2)
        val lt = trig(pLtAxis, pLtKey, vLt); val rt = trig(pRtAxis, pRtKey, vRt)
        val btn = pBtn or vBtn; val hat = pKeyHat or pAxisHat or vHat
        // Keyboard/mouse output (default): same merged state, X11 events instead of the virtual pad.
        ControllerInput.update(lx, ly, rx, ry, lt, rt, btn, hat)
        val b = buf ?: return
        if (!ControllerInput.gamepadOutput) return
        b.putShort(8, s16(lx)); b.putShort(10, s16(ly)); b.putShort(12, s16(rx)); b.putShort(14, s16(ry))
        b.putShort(16, s16(lt)); b.putShort(18, s16(rt))
        b.putShort(20, btn.toShort())
        b.put(22, hat.toByte())
        b.putInt(4, connected)
        b.putInt(0, b.getInt(0) + 1) // seq last
    }
}
