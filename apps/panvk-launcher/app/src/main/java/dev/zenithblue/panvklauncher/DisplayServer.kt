package dev.zenithblue.panvklauncher

import android.content.Context
import android.content.Intent
import android.net.LocalSocket
import android.net.LocalSocketAddress
import java.io.File

/**
 * Display backends, selected in settings (pref `display_mode`):
 *  - BUILTIN (default): in-process Winlator Java X server, surface in [XServerActivity].
 *  - TERMUX: external Termux:X11 (com.termux.x11). Surface and input live in that app.
 *    We publish DISPLAY and the TMPDIR of the live socket Wine's libX11 will open,
 *    and start the server when that directory is writable by us.
 */
object DisplayServer {
    const val PACKAGE = "com.termux.x11"
    private const val PREFS = "launcher"
    private val ownedLock = Any()
    private var ownedProc: Process? = null

    enum class Mode { BUILTIN, TERMUX }

    data class Session(
        val display: String,
        val socketPath: String,
        val env: Map<String, String>,
        val owned: Boolean
    )

    private fun prefs(ctx: Context) = ctx.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun mode(ctx: Context): Mode =
        if (prefs(ctx).getString("display_mode", "builtin") == "termux") Mode.TERMUX else Mode.BUILTIN

    fun setMode(ctx: Context, mode: Mode) {
        prefs(ctx).edit().putString("display_mode", if (mode == Mode.TERMUX) "termux" else "builtin").apply()
    }

    fun resolution(ctx: Context): String =
        prefs(ctx).getString("display_res", BuiltinXServer.DEFAULT_RESOLUTION) ?: BuiltinXServer.DEFAULT_RESOLUTION

    fun setResolution(ctx: Context, res: String) {
        prefs(ctx).edit().putString("display_res", res).apply()
    }

    // MIT-SHM through the SysV broker. Off = plain PutImage over the socket.
    fun useShm(ctx: Context): Boolean = prefs(ctx).getBoolean("display_shm", true)

    fun setShm(ctx: Context, on: Boolean) {
        prefs(ctx).edit().putBoolean("display_shm", on).apply()
    }

    fun describe(ctx: Context): String {
        if (mode(ctx) == Mode.BUILTIN) {
            val state = if (BuiltinXServer.isRunning()) "running" else "idle"
            return "Display: built-in X server ${resolution(ctx)} ($state)"
        }
        if (!isInstalled(ctx)) return "Display: Termux:X11 not installed"
        val live = findLive(ctx)
        return if (live != null) "Display: ${live.first} @ ${live.second}"
        else "Display: Termux:X11 installed, no X socket"
    }

    fun openBuiltin(ctx: Context): String? {
        if (!BuiltinXServer.isRunning()) return "Built-in X server is not running. Start a graphical run first."
        return try {
            ctx.startActivity(
                Intent(ctx, XServerActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_REORDER_TO_FRONT)
            )
            null
        } catch (t: Throwable) {
            "Could not open display: ${t.message}"
        }
    }

    fun open(ctx: Context): String? {
        if (mode(ctx) == Mode.BUILTIN) return openBuiltin(ctx)
        if (!isInstalled(ctx)) {
            return "Termux:X11 ($PACKAGE) is not installed. Install the Termux:X11 app, then start it before a graphical launch."
        }
        val intent = ctx.packageManager.getLaunchIntentForPackage(PACKAGE)
            ?: return "Termux:X11 is installed but has no launch activity."
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        return try {
            ctx.startActivity(intent)
            null
        } catch (t: Throwable) {
            "Could not open Termux:X11: ${t.message}"
        }
    }

    fun prepare(ctx: Context, onLine: (String) -> Unit): Session? {
        if (mode(ctx) == Mode.BUILTIN) {
            val session = BuiltinXServer.start(ctx, resolution(ctx), useShm(ctx), onLine) ?: return null
            val err = openBuiltin(ctx)
            if (err != null) onLine(err) else onLine("Display activity opened (surface and input).")
            return session
        }
        if (!isInstalled(ctx)) {
            onLine("Display server missing: install Termux:X11 ($PACKAGE).")
            onLine("The framebuffer view is not an X server and cannot serve DISPLAY.")
            return null
        }
        val templates = templates(ctx)
        var live = findLive(templates)
        var owned = false
        if (live == null) {
            owned = startOwned(ctx, templates, onLine)
            if (owned) live = findLive(templates)
        }
        if (live == null) {
            onLine("Termux:X11 is installed but not listening.")
            for (template in templates) {
                onLine("Wine libX11 looks for ${template}<0-9> (server TMPDIR=${DisplayPaths.tmpdirOf(template)}).")
            }
            onLine("Start the server, then relaunch. Example from Termux:")
            onLine("  termux-x11 :0")
            onLine("The X socket must be the path above. A framebuffer viewer is not enough.")
            return null
        }
        val (display, path) = live
        val openErr = open(ctx)
        if (openErr != null) onLine(openErr)
        else onLine("Termux:X11 activity opened (surface and input).")
        val env = linkedMapOf(
            "DISPLAY" to ":$display",
            "TMPDIR" to DisplayPaths.tmpdirOfSocket(path)
        )
        xkbRoot(ctx)?.let { env["XKB_CONFIG_ROOT"] = it }
        onLine("DISPLAY=:$display socket=$path TMPDIR=${env["TMPDIR"]}")
        return Session(":$display", path, env, owned)
    }

    fun stopOwned() {
        BuiltinXServer.stop()
        val proc = synchronized(ownedLock) {
            val p = ownedProc
            ownedProc = null
            p
        } ?: return
        try {
            proc.destroyForcibly()
        } catch (_: Exception) {}
    }

    fun close(session: Session) {
        if (session.owned) stopOwned()
    }

    private fun isInstalled(ctx: Context): Boolean = try {
        ctx.packageManager.getPackageInfo(PACKAGE, 0)
        true
    } catch (_: Exception) {
        false
    }

    // libX11 joins getenv("TMPDIR") ?: "/tmp" with ".X11-unix/X<n>".
    // Container env defaults TMPDIR to imagefs usr/tmp. A /tmp socket needs
    // that overridden or Wine never sees the server we selected.
    private fun templates(ctx: Context): List<String> {
        val wineTmp = File(ctx.filesDir, "contents/imagefs/bionic/usr/tmp")
        return listOf(
            File(wineTmp, ".X11-unix/X").absolutePath,
            "/tmp/.X11-unix/X"
        )
    }

    private fun findLive(ctx: Context): Pair<String, String>? {
        val hit = findLive(templates(ctx)) ?: return null
        return ":${hit.first}" to hit.second
    }

    private fun findLive(templates: List<String>): Pair<Int, String>? =
        DisplayPaths.pick(templates) { isListening(it) }

    internal fun isListening(path: String): Boolean {
        if (!File(path).exists()) return false
        val socket = LocalSocket()
        return try {
            socket.connect(LocalSocketAddress(path, LocalSocketAddress.Namespace.FILESYSTEM))
            true
        } catch (_: Exception) {
            false
        } finally {
            try {
                socket.close()
            } catch (_: Exception) {}
        }
    }

    private fun xkbRoot(ctx: Context): String? {
        val imagefs = File(ctx.filesDir, "contents/imagefs/bionic")
        val candidates = listOf(
            File(imagefs, "usr/share/X11/xkb"),
            File(imagefs, "usr/share/xkeyboard-config-2"),
            File("/data/data/com.termux/files/usr/share/X11/xkb"),
            File("/data/data/com.termux/files/usr/share/xkeyboard-config-2")
        )
        return candidates.firstOrNull { it.isDirectory }?.absolutePath
    }

    private fun startOwned(ctx: Context, templates: List<String>, onLine: (String) -> Unit): Boolean {
        val xkb = xkbRoot(ctx)
        if (xkb == null) {
            onLine("Not starting Termux:X11: XKB_CONFIG_ROOT missing (imagefs usr/share/X11/xkb or Termux xkeyboard-config).")
            return false
        }
        val apk = try {
            ctx.packageManager.getPackageInfo(PACKAGE, 0).applicationInfo?.sourceDir
        } catch (_: Exception) {
            null
        }
        if (apk.isNullOrEmpty()) {
            onLine("Not starting Termux:X11: apk path unavailable.")
            return false
        }
        for (template in templates) {
            val tmp = File(DisplayPaths.tmpdirOf(template))
            if (!tmp.isDirectory && !tmp.mkdirs()) continue
            if (!tmp.canWrite()) continue
            onLine("Starting Termux:X11 :0 TMPDIR=${tmp.absolutePath}")
            val proc = try {
                val pb = ProcessBuilder(
                    "/system/bin/app_process",
                    "/",
                    "--nice-name=termux-x11",
                    "com.termux.x11.CmdEntryPoint",
                    ":0"
                )
                val keep = arrayOf(
                    "ANDROID_ART_ROOT", "ANDROID_DATA", "ANDROID_I18N_ROOT",
                    "ANDROID_ROOT", "ANDROID_TZDATA_ROOT", "BOOTCLASSPATH"
                )
                val saved = keep.associateWith { pb.environment()[it] }
                val env = pb.environment()
                env.clear()
                for ((k, v) in saved) if (v != null) env[k] = v
                env["CLASSPATH"] = apk
                env["TMPDIR"] = tmp.absolutePath
                env["XKB_CONFIG_ROOT"] = xkb
                env["PATH"] = "/system/bin:/system/xbin"
                env["ANDROID_ROOT"] = "/system"
                env["ANDROID_DATA"] = "/data"
                env["HOME"] = tmp.absolutePath
                pb.redirectErrorStream(true)
                pb.start()
            } catch (t: Throwable) {
                onLine("Termux:X11 start failed: ${t.message}")
                continue
            }
            synchronized(ownedLock) { ownedProc = proc }
            Thread({
                try {
                    proc.inputStream.bufferedReader().use { reader ->
                        reader.lineSequence().forEach { onLine("x11: $it") }
                    }
                } catch (_: Exception) {}
            }, "termux-x11-log").apply { isDaemon = true }.start()
            val sock = File(tmp, ".X11-unix/X0")
            val deadline = System.currentTimeMillis() + 4000
            while (System.currentTimeMillis() < deadline) {
                if (!proc.isAlive) {
                    onLine("Termux:X11 exited before ${sock.absolutePath} appeared.")
                    synchronized(ownedLock) { if (ownedProc == proc) ownedProc = null }
                    break
                }
                if (isListening(sock.absolutePath)) return true
                try {
                    Thread.sleep(100)
                } catch (_: InterruptedException) {
                    break
                }
            }
            onLine("Termux:X11 did not listen on ${sock.absolutePath} within 4s.")
            try {
                proc.destroyForcibly()
            } catch (_: Exception) {}
            synchronized(ownedLock) { if (ownedProc == proc) ownedProc = null }
        }
        return false
    }
}
