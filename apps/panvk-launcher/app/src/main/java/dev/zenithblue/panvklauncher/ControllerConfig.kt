// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.content.Context
import com.winlator.xserver.Pointer
import com.winlator.xserver.XKeycode
import org.json.JSONObject
import java.io.File

/**
 * Per-game controller mapping: overlay controls and physical pads -> X11 keys / pointer buttons / relative
 * mouse motion. JSON, see docs/GAME-SHORTCUTS.md. Resolution order at launch ([resolve]):
 * files/controller/<shortcutId>.json, then the asset preset (controller-presets dir) whose "exe" list
 * contains the exe file name (copied into files/controller so it is editable), then controller-presets/default.json.
 *
 * A binding is "none", a key name from [ControllerKeys] ("w", "space", "Escape", "Shift_L"), or "mouse:left|right|middle|wheelup|wheeldown".
 */
class StickConfig(
    val mode: String = "keys", // keys | mouse
    val up: String = "w", val down: String = "s", val left: String = "a", val right: String = "d",
    val sensitivity: Float = 1f, // mouse mode: 1.0 = 900 px/s (X screen px) at full deflection
    val deadzone: Float = 0.3f,
    val invertY: Boolean = false
)

class ControllerConfig(
    val name: String,
    val output: String, // keyboard (default) | gamepad (virtual XInput pad, old behaviour) | both
    val bindings: Map<String, String>,
    val labels: Map<String, String>,
    val leftStick: StickConfig,
    val rightStick: StickConfig,
    val exe: List<String> = emptyList()
) {
    fun toJson(): JSONObject = JSONObject().apply {
        put("name", name); put("output", output)
        if (exe.isNotEmpty()) put("exe", org.json.JSONArray(exe))
        put("bindings", JSONObject(bindings)); put("labels", JSONObject(labels))
        put("leftStick", stickJson(leftStick)); put("rightStick", stickJson(rightStick))
    }

    fun with(
        bindings: Map<String, String> = this.bindings, labels: Map<String, String> = this.labels,
        leftStick: StickConfig = this.leftStick, rightStick: StickConfig = this.rightStick, output: String = this.output
    ) = ControllerConfig(name, output, bindings, labels, leftStick, rightStick, exe)

    /** Text drawn on an overlay control: label if set, else a short form of the binding. */
    fun caption(id: String): String = labels[id]?.takeIf { it.isNotEmpty() } ?: ControllerKeys.short(bindings[id] ?: "none")

    /** "WASD" for single-letter key sticks, "MOUSE" for mouse mode, else "". */
    fun stickCaption(left: Boolean): String {
        val s = if (left) leftStick else rightStick
        if (s.mode == "mouse") return "MOUSE"
        val k = listOf(s.up, s.left, s.down, s.right).map { ControllerKeys.short(it) }
        return if (k.all { it.length == 1 }) k.joinToString("") else ""
    }

    companion object {
        val IDS = listOf(
            "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "BACK", "START", "L3", "R3",
            "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT"
        )
        const val DIR = "controller"
        const val ASSETS = "controller-presets"

        private fun stickJson(s: StickConfig) = JSONObject().apply {
            put("mode", s.mode); put("up", s.up); put("down", s.down); put("left", s.left); put("right", s.right)
            put("sensitivity", s.sensitivity.toDouble()); put("deadzone", s.deadzone.toDouble()); put("invertY", s.invertY)
        }

        private fun stickFrom(j: JSONObject?, d: StickConfig) = if (j == null) d else StickConfig(
            mode = j.optString("mode", d.mode), up = j.optString("up", d.up), down = j.optString("down", d.down),
            left = j.optString("left", d.left), right = j.optString("right", d.right),
            sensitivity = j.optDouble("sensitivity", d.sensitivity.toDouble()).toFloat(),
            deadzone = j.optDouble("deadzone", d.deadzone.toDouble()).toFloat().coerceIn(0.02f, 0.9f),
            invertY = j.optBoolean("invertY", d.invertY)
        )

        private fun map(j: JSONObject?): Map<String, String> {
            val m = LinkedHashMap<String, String>()
            j?.keys()?.forEach { m[it] = j.optString(it, "none") }
            return m
        }

        /** Built-in fallback when assets are missing: WASD + mouse. */
        val FALLBACK = ControllerConfig(
            "default", "keyboard",
            mapOf(
                "A" to "space", "B" to "Escape", "X" to "e", "Y" to "f", "LB" to "q", "RB" to "r",
                "LT" to "mouse:right", "RT" to "mouse:left", "BACK" to "Tab", "START" to "Escape",
                "L3" to "Shift_L", "R3" to "c",
                "DPAD_UP" to "Up", "DPAD_DOWN" to "Down", "DPAD_LEFT" to "Left", "DPAD_RIGHT" to "Right"
            ),
            mapOf("A" to "Jump", "B" to "Esc", "X" to "Use", "Y" to "F", "LT" to "RMB", "RT" to "LMB", "L3" to "Run"),
            StickConfig("keys"), StickConfig("mouse", sensitivity = 1f, deadzone = 0.12f)
        )

        fun fromJson(j: JSONObject): ControllerConfig {
            val d = FALLBACK
            val exes = j.optJSONArray("exe")?.let { a -> (0 until a.length()).map { a.getString(it) } } ?: emptyList()
            return ControllerConfig(
                j.optString("name", "custom"), j.optString("output", "keyboard"),
                d.bindings + map(j.optJSONObject("bindings")), map(j.optJSONObject("labels")),
                stickFrom(j.optJSONObject("leftStick"), d.leftStick), stickFrom(j.optJSONObject("rightStick"), d.rightStick), exes
            )
        }

        fun file(ctx: Context, shortcutId: String) = File(File(ctx.filesDir, DIR).apply { mkdirs() }, "$shortcutId.json")

        fun save(ctx: Context, shortcutId: String, c: ControllerConfig) {
            val f = file(ctx, shortcutId)
            val tmp = File(f.path + ".tmp")
            tmp.writeText(c.toJson().toString(2)); tmp.renameTo(f)
        }

        private fun asset(ctx: Context, n: String): ControllerConfig? = try {
            fromJson(JSONObject(ctx.assets.open("$ASSETS/$n").bufferedReader().use { it.readText() }))
        } catch (_: Exception) { null }

        fun presetFor(ctx: Context, exePath: String): ControllerConfig? {
            val base = File(exePath.replace('\\', '/')).name
            return (ctx.assets.list(ASSETS) ?: emptyArray()).sorted().firstNotNullOfOrNull { n ->
                asset(ctx, n)?.takeIf { c -> c.exe.any { it.equals(base, true) } }
            }
        }

        fun default(ctx: Context) = asset(ctx, "default.json") ?: FALLBACK

        /** Give a shortcut whose exe matches a preset (e.g. MiSide.exe) its own editable controller file. */
        fun attachPreset(ctx: Context, shortcutId: String, exePath: String) {
            if (!file(ctx, shortcutId).isFile) presetFor(ctx, exePath)?.let { try { save(ctx, shortcutId, it) } catch (_: Exception) {} }
        }

        /** Config for a game: saved file, else matching preset (materialised into the file), else default. */
        fun resolve(ctx: Context, shortcutId: String?, exePath: String): ControllerConfig {
            if (shortcutId != null) {
                val f = file(ctx, shortcutId)
                if (f.isFile) try { return fromJson(JSONObject(f.readText())) } catch (_: Exception) {}
            }
            val p = presetFor(ctx, exePath)
            if (p != null) {
                if (shortcutId != null) try { save(ctx, shortcutId, p) } catch (_: Exception) {}
                return p
            }
            return default(ctx)
        }
    }
}

/** Key name <-> XKeycode. Names are X keysym names, case-insensitive; the X server here knows the keys below only. */
object ControllerKeys {
    private val names = LinkedHashMap<String, XKeycode>().apply {
        for (c in 'a'..'z') put(c.toString(), XKeycode.valueOf("KEY_${c.uppercaseChar()}"))
        for (d in 0..9) put(d.toString(), XKeycode.valueOf("KEY_$d"))
        put("space", XKeycode.KEY_SPACE); put("Escape", XKeycode.KEY_ESC); put("Return", XKeycode.KEY_ENTER)
        put("Tab", XKeycode.KEY_TAB); put("BackSpace", XKeycode.KEY_BKSP)
        put("Shift_L", XKeycode.KEY_SHIFT_L); put("Shift_R", XKeycode.KEY_SHIFT_R)
        put("Control_L", XKeycode.KEY_CTRL_L); put("Control_R", XKeycode.KEY_CTRL_R)
        put("Alt_L", XKeycode.KEY_ALT_L); put("Alt_R", XKeycode.KEY_ALT_R)
        put("Up", XKeycode.KEY_UP); put("Down", XKeycode.KEY_DOWN); put("Left", XKeycode.KEY_LEFT); put("Right", XKeycode.KEY_RIGHT)
        for (f in 1..12) put("F$f", XKeycode.valueOf("KEY_F$f"))
        put("Home", XKeycode.KEY_HOME); put("End", XKeycode.KEY_END); put("Prior", XKeycode.KEY_PRIOR)
        put("Next", XKeycode.KEY_NEXT); put("Insert", XKeycode.KEY_INSERT); put("Delete", XKeycode.KEY_DEL)
        put("minus", XKeycode.KEY_MINUS); put("equal", XKeycode.KEY_EQUAL); put("comma", XKeycode.KEY_COMMA)
        put("period", XKeycode.KEY_PERIOD); put("slash", XKeycode.KEY_SLASH); put("backslash", XKeycode.KEY_BACKSLASH)
        put("semicolon", XKeycode.KEY_SEMICOLON); put("apostrophe", XKeycode.KEY_APOSTROPHE); put("grave", XKeycode.KEY_GRAVE)
        put("bracketleft", XKeycode.KEY_BRACKET_LEFT); put("bracketright", XKeycode.KEY_BRACKET_RIGHT)
    }
    private val lower = names.mapKeys { it.key.lowercase() }
    private val alias = mapOf(
        "esc" to "escape", "enter" to "return", "ctrl" to "control_l", "control" to "control_l", "shift" to "shift_l",
        "alt" to "alt_l", "backspace" to "backspace", "page_up" to "prior", "pageup" to "prior", "page_down" to "next",
        "pagedown" to "next", "del" to "delete"
    )
    val MOUSE = listOf("mouse:left", "mouse:right", "mouse:middle", "mouse:wheelup", "mouse:wheeldown")

    /** Choices for the editor: none, mouse actions, keys. */
    val CHOICES: List<String> = listOf("none") + MOUSE + names.keys

    fun key(b: String): XKeycode? = lower[b.lowercase()] ?: alias[b.lowercase()]?.let { lower[it] }

    fun mouse(b: String): Pointer.Button? = when (b.lowercase()) {
        "mouse:left" -> Pointer.Button.BUTTON_LEFT
        "mouse:right" -> Pointer.Button.BUTTON_RIGHT
        "mouse:middle" -> Pointer.Button.BUTTON_MIDDLE
        "mouse:wheelup" -> Pointer.Button.BUTTON_SCROLL_UP
        "mouse:wheeldown" -> Pointer.Button.BUTTON_SCROLL_DOWN
        else -> null
    }

    /** Short caption for overlay buttons. */
    fun short(b: String): String = when {
        b == "none" || b.isEmpty() -> ""
        b.startsWith("mouse:") -> when (b.substring(6).lowercase()) {
            "left" -> "LMB"; "right" -> "RMB"; "middle" -> "MMB"; "wheelup" -> "W+"; "wheeldown" -> "W-"; else -> "M"
        }
        else -> when (val k = key(b)?.let { c -> names.entries.first { it.value == c }.key } ?: b) {
            "space" -> "SPC"; "Escape" -> "ESC"; "Return" -> "ENT"; "BackSpace" -> "BKSP"
            "Shift_L", "Shift_R" -> "SHIFT"; "Control_L", "Control_R" -> "CTRL"; "Alt_L", "Alt_R" -> "ALT"
            "Up" -> "UP"; "Down" -> "DOWN"; "Left" -> "LEFT"; "Right" -> "RIGHT"
            else -> k.uppercase()
        }
    }
}
