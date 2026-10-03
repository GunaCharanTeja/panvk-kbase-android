// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.RandomAccessFile
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

/**
 * One directory per game session under files/sessions/<time>_<name>/ with every log we can get, written when the
 * Wine run returns (user EXIT, game crash or driver failure alike). [SessionLogsActivity] shows it, [zip] bundles it.
 * Files: summary.txt, session.json, wine-run.log (stdout+stderr incl. WINEDEBUG/DXVK/Mesa stderr), dxvk-*.log,
 * unity-Player.log, mesa-panvk.txt (filtered), xserver.log, launcher.log, logcat.txt, device.txt, config.txt.
 */
object SessionLogs {
    const val KEEP = 10
    private const val MAX_FILE = 4L shl 20      // per file, tail kept
    private const val MAX_LOGCAT = 2L shl 20    // "last 2 MB" of logcat
    private val ERR = Regex(
        "(?i)(\\berr:|\\berror\\b|\\bfatal\\b|exception|crash|segfault|sigsegv|sigabrt|sigbus|device[_ ]lost|unhandled|\\bassert|\\babort|page fault|\\bfailed\\b|backtrace)"
    )
    private val WARN = Regex("(?i)(\\bwarn|\\bwarning\\b|\\bwarn:)")
    private val MESA = Regex("(?i)(panvk|mesa|panfrost|libvulkan_panfrost|kbase|\\bmali\\b|vk_error|vkcreate|vkqueue|device[_ ]lost)")
    fun isError(l: String) = ERR.containsMatchIn(l)
    fun isWarn(l: String) = WARN.containsMatchIn(l)

    fun root(ctx: Context) = File(ctx.filesDir, "sessions").apply { mkdirs() }

    /** Newest first. */
    fun list(ctx: Context): List<File> =
        (root(ctx).listFiles { f -> f.isDirectory } ?: emptyArray()).sortedByDescending { it.name }

    fun summary(dir: File): JSONObject = try { JSONObject(File(dir, "session.json").readText()) } catch (_: Exception) { JSONObject() }

    private fun tail(src: File, max: Long): String {
        if (!src.isFile) return ""
        return try {
            RandomAccessFile(src, "r").use { f ->
                val n = f.length()
                val start = if (n > max) n - max else 0
                f.seek(start)
                val b = ByteArray((n - start).toInt()); f.readFully(b)
                (if (start > 0) "[... ${start} earlier bytes cut, last ${max shr 20} MB kept ...]\n" else "") + String(b, Charsets.UTF_8)
            }
        } catch (t: Throwable) { "[read failed: $t]\n" }
    }

    private fun sig(n: Int) = mapOf(
        1 to "SIGHUP", 2 to "SIGINT", 3 to "SIGQUIT", 4 to "SIGILL", 6 to "SIGABRT", 7 to "SIGBUS", 8 to "SIGFPE",
        9 to "SIGKILL", 11 to "SIGSEGV", 13 to "SIGPIPE", 15 to "SIGTERM"
    )[n] ?: "signal $n"

    fun collect(
        ctx: Context, sc: Shortcut?, exePath: String, startMs: Long, launcherLog: String, ok: Boolean
    ): File {
        val now = System.currentTimeMillis()
        val label = (sc?.name ?: File(exePath).nameWithoutExtension).replace(Regex("[^A-Za-z0-9._-]+"), "_").take(40)
        val dir = File(root(ctx), SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date(now)) + "_" + label).apply { mkdirs() }
        val run = ContainerManager.lastRun?.takeIf { it.startMs >= startMs - 1000 }
        val errors = ArrayList<String>()
        fun save(name: String, text: String) {
            if (text.isEmpty()) return
            File(dir, name).writeText(text)
            if (name != "device.txt" && name != "config.txt")
                for (l in text.lineSequence()) if (errors.size < 400 && isError(l) && l.length < 600) errors += "[$name] ${l.trim()}"
        }

        // Wine stdout/stderr (WINEDEBUG channels, DXVK and Mesa/PanVK stderr land here).
        val wineLog = run?.logFile?.let { tail(it, MAX_FILE) } ?: ""
        save("wine-run.log", wineLog)
        save("launcher.log", launcherLog)

        // DXVK logs next to the exe (cwd), written this run.
        val exeFile = File(exePath)
        exeFile.parentFile?.listFiles { f -> f.isFile && f.name.endsWith(".log") && (f.name.contains("_d3d") || f.name.contains("_dxgi")) }
            ?.filter { it.lastModified() >= startMs - 5000 }?.forEach { save("dxvk-${it.name}", tail(it, MAX_FILE)) }

        // Unity Player.log (AppData/LocalLow/<company>/<product>/Player.log) inside the prefix.
        val users = File(ctx.filesDir, "container/.wine/drive_c/users")
        users.listFiles()?.forEach { u ->
            File(u, "AppData/LocalLow").listFiles()?.forEach { co -> co.listFiles()?.forEach { prod ->
                for (n in listOf("Player.log", "Player-prev.log")) File(prod, n).takeIf { it.isFile }?.let {
                    val stale = if (it.lastModified() < startMs - 60_000) " (older than this run)" else ""
                    save("unity-${n.removeSuffix(".log")}-${prod.name}.log", "[${it.path} modified ${Date(it.lastModified())}$stale]\n" + tail(it, MAX_FILE))
                }
            } }
        }
        save("xserver.log", tail(File(ctx.filesDir, "xserver.log"), MAX_FILE))

        // logcat (own uid only on Android; last 2 MB).
        val logcat = try {
            val p = ProcessBuilder("logcat", "-d", "-v", "threadtime", "-t", "40000").redirectErrorStream(true).start()
            val text = p.inputStream.bufferedReader().use { it.readText() }
            p.waitFor()
            if (text.length > MAX_LOGCAT) text.takeLast(MAX_LOGCAT.toInt()) else text
        } catch (t: Throwable) { "logcat failed: $t\n" }
        File(dir, "logcat.txt").writeText(logcat)
        for (l in logcat.lineSequence()) if (errors.size < 400 && (" E " in l || " F " in l) && isError(l) && l.length < 600) errors += "[logcat] ${l.trim()}"

        // Mesa / PanVK lines from everything.
        val mesa = (wineLog.lineSequence() + logcat.lineSequence()).filter { MESA.containsMatchIn(it) }.take(5000).joinToString("\n")
        save("mesa-panvk.txt", mesa)
        val tomb = Regex("Tombstone written to:?\\s*(\\S+)|(/data/tombstones/\\S+)").find(logcat)?.let { it.groupValues[1].ifEmpty { it.groupValues[2] } }

        // Exit reason.
        val exit = run?.exit ?: Int.MIN_VALUE
        val deviceLost = Regex("(?i)device[_ ]lost").containsMatchIn(wineLog + logcat)
        var crash = false
        val reason = when {
            run == null -> if (ok) "No Wine run recorded" else "Launch failed before Wine started"
            ContainerManager.userStopped -> "User exit (overlay EXIT or Stop)"
            exit == 0 -> "Game exited normally (exit 0)"
            exit > 128 -> { crash = true; "Crash: Wine killed by ${sig(exit - 128)} (exit $exit)" }
            exit == Int.MIN_VALUE -> { crash = true; "Run aborted (no exit code)" }
            else -> { crash = true; "Abnormal exit (code $exit)" }
        } + if (deviceLost) " + driver failure: VK_ERROR_DEVICE_LOST seen in logs" else ""

        save("device.txt", deviceText(ctx, sc, tomb))
        save("config.txt", configText(ctx, sc, exePath, run))

        val dur = ((run?.endMs?.takeIf { it > 0 } ?: now) - (run?.startMs ?: startMs)) / 1000
        val uniq = errors.distinct()
        val sum = JSONObject().apply {
            put("time", now); put("startMs", run?.startMs ?: startMs); put("durationSec", dur)
            put("game", sc?.name ?: exeFile.name); put("exe", exePath); put("reason", reason)
            put("exit", if (exit == Int.MIN_VALUE) JSONObject.NULL else exit)
            put("userStopped", ContainerManager.userStopped); put("crash", crash || deviceLost)
            put("deviceLost", deviceLost); put("tombstone", tomb ?: JSONObject.NULL)
            put("errorCount", uniq.size); put("errors", JSONArray(uniq.take(60)))
            put("files", JSONArray((dir.listFiles() ?: emptyArray()).map { it.name }.sorted()))
        }
        File(dir, "session.json").writeText(sum.toString(2))
        File(dir, "summary.txt").writeText(buildString {
            appendLine("Game: ${sum.getString("game")}"); appendLine("Exe: $exePath")
            appendLine("Exit reason: $reason"); appendLine("Wine exit code: ${if (exit == Int.MIN_VALUE) "n/a" else exit}")
            appendLine("Duration: ${dur}s"); appendLine("Tombstone: ${tomb ?: "none seen in logcat (apps cannot read /data/tombstones)"}")
            appendLine("Errors found: ${uniq.size}"); uniq.take(60).forEach { appendLine("  $it") }
        })
        prune(ctx)
        try { zip(ctx, dir) } catch (_: Throwable) {}
        return dir
    }

    private fun deviceText(ctx: Context, sc: Shortcut?, tomb: String?) = buildString {
        appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE}), Android ${Build.VERSION.RELEASE} SDK ${Build.VERSION.SDK_INT}")
        appendLine("Build: ${Build.FINGERPRINT}"); appendLine("ABI: ${Build.SUPPORTED_ABIS.joinToString()}")
        appendLine("Hardware: ${Build.HARDWARE} / ${Build.BOARD}")
        val dm = ctx.resources.displayMetrics
        appendLine("Display: ${dm.widthPixels}x${dm.heightPixels} dpi ${dm.densityDpi}; X screen ${DisplayServer.resolution(ctx)}")
        val drivers = DriverManager.getDrivers(ctx)
        val d = sc?.driver?.takeIf { it.isNotEmpty() }?.let { id -> drivers.firstOrNull { it.id == id } } ?: DriverManager.getSelectedDriver(ctx, drivers)
        val lib = File(d.libPath)
        appendLine("Driver: ${d.name} (${d.id}) version ${d.version} bundled=${d.bundled}")
        appendLine("ICD library: ${d.libPath} size=${lib.length()} modified=${Date(lib.lastModified())}")
        appendLine("ICD ELF build id: ${elfBuildId(lib) ?: "n/a"}")
        appendLine("ICD json: ${File(ctx.filesDir, "container/panvk_icd.json").takeIf { it.isFile }?.readText()?.replace("\n", " ")}")
        appendLine("Launcher: pid ${android.os.Process.myPid()}, DXVK enabled=${ContainerManager.isDxvkEnabled(ctx)}")
        appendLine("Tombstone: ${tomb ?: "none"}")
        if (Build.VERSION.SDK_INT >= 30) try {
            val am = ctx.getSystemService(ActivityManager::class.java)
            for (e in am.getHistoricalProcessExitReasons(ctx.packageName, 0, 5)) appendLine("ExitInfo: ${Date(e.timestamp)} pid ${e.pid} reason ${e.reason} status ${e.status} ${e.description}")
        } catch (_: Throwable) {}
    }

    private fun configText(ctx: Context, sc: Shortcut?, exePath: String, run: ContainerManager.RunInfo?) = buildString {
        appendLine("== Shortcut ==")
        appendLine(sc?.let { File(ctx.filesDir, "shortcuts/${it.id}.json").takeIf { f -> f.isFile }?.readText() } ?: "(none, plain exe run: $exePath)")
        appendLine("\n== Controller config ==")
        appendLine(ControllerInput.config.toJson().toString(2))
        appendLine("\n== Wine command ==")
        appendLine(run?.command?.joinToString(" ") ?: "n/a")
        appendLine("\n== Environment (as passed to Wine) ==")
        run?.env?.toSortedMap()?.forEach { (k, v) -> appendLine("$k=$v") }
    }

    private fun elfBuildId(f: File): String? = try {
        RandomAccessFile(f, "r").use { r ->
            fun u(o: Long, n: Int): Long { r.seek(o); var v = 0L; for (i in 0 until n) v = v or (r.read().toLong() shl (8 * i)); return v }
            val phoff = u(0x20, 8); val phent = u(0x36, 2); val phnum = u(0x38, 2)
            var id: String? = null
            for (i in 0 until phnum) {
                val ph = phoff + i * phent
                if (u(ph, 4) != 4L) continue
                var o = u(ph + 8, 8); val end = o + u(ph + 32, 8)
                while (o + 12 <= end) {
                    val nsz = u(o, 4); val dsz = u(o + 4, 4); val ty = u(o + 8, 4)
                    val d = o + 12 + ((nsz + 3) and 3L.inv())
                    if (ty == 3L && nsz == 4L) { r.seek(d); val b = ByteArray(dsz.toInt()); r.readFully(b); id = b.joinToString("") { "%02x".format(it) } }
                    o = d + ((dsz + 3) and 3L.inv())
                }
            }
            id
        }
    } catch (_: Throwable) { null }

    private fun prune(ctx: Context) {
        list(ctx).drop(KEEP).forEach { d ->
            d.deleteRecursively(); File(zipDir(ctx), d.name + ".zip").delete()
        }
    }

    fun zipDir(ctx: Context) = File(ctx.cacheDir, "session-zips").apply { mkdirs() }

    /** Zip of one session dir in cache/session-zips (FileProvider path). Rebuilt when missing. */
    fun zip(ctx: Context, dir: File): File {
        val z = File(zipDir(ctx), dir.name + ".zip")
        if (z.isFile && z.lastModified() >= (dir.listFiles()?.maxOfOrNull { it.lastModified() } ?: 0)) return z
        ZipOutputStream(z.outputStream().buffered()).use { out ->
            for (f in (dir.listFiles() ?: emptyArray()).sortedBy { it.name }) {
                if (!f.isFile) continue
                out.putNextEntry(ZipEntry(dir.name + "/" + f.name)); f.inputStream().use { it.copyTo(out) }; out.closeEntry()
            }
        }
        return z
    }
}
