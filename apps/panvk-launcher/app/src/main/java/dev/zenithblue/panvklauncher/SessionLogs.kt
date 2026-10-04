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

        val manifestInfo = try {
            currentManifestInfo(ctx, sc, exePath, exit, reason, dur)
        } catch (t: Throwable) {
            JSONObject().apply { put("error", t.message ?: t.toString()) }
        }

        val sum = JSONObject().apply {
            put("time", now); put("startMs", run?.startMs ?: startMs); put("durationSec", dur)
            put("game", sc?.name ?: exeFile.name); put("exe", exePath); put("reason", reason)
            put("exit", if (exit == Int.MIN_VALUE) JSONObject.NULL else exit)
            put("userStopped", ContainerManager.userStopped); put("crash", crash || deviceLost)
            put("deviceLost", deviceLost); put("tombstone", tomb ?: JSONObject.NULL)
            put("errorCount", uniq.size); put("errors", JSONArray(uniq.take(60)))
            put("files", JSONArray((dir.listFiles() ?: emptyArray()).map { it.name }.sorted()))
            put("manifestInfo", manifestInfo)
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
        appendLine("\n== FEX mode ==")
        appendLine(FexPresets.resolve(sc?.fex ?: ""))
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
        val zips = zipDir(ctx)
        list(ctx).drop(KEEP).forEach { d ->
            d.deleteRecursively()
            zips.listFiles()?.forEach { f ->
                if (f.isFile && (f.name == "${d.name}.zip" || f.name.startsWith("${d.name}-"))) {
                    f.delete()
                }
            }
        }
    }

    fun getWineVersion(ctx: Context): JSONObject {
        try {
            val containerJson = File(ctx.filesDir, "container/container.json")
            if (containerJson.isFile) {
                val winePath = JSONObject(containerJson.readText()).optString("wine", "")
                if (winePath.isNotEmpty()) {
                    val profile = File(winePath, "profile.json")
                    if (profile.isFile) {
                        val ver = JSONObject(profile.readText()).optString("versionName", "")
                        if (ver.isNotEmpty()) {
                            return JSONObject().apply {
                                put("version", ver)
                                put("source", "container")
                            }
                        }
                    }
                    val dirName = File(winePath).name
                    if (dirName.isNotEmpty()) {
                        return JSONObject().apply {
                            put("version", dirName)
                            put("source", "container")
                        }
                    }
                }
            }
        } catch (_: Exception) {}

        val installedVer = ContentManager.current(ctx, "Proton")?.versionName
            ?: ContentManager.list(ctx).firstOrNull { it.type == "Proton" && ContentManager.isComplete(it) }?.versionName
            ?: ContentManager.list(ctx).firstOrNull { it.type == "Proton" }?.versionName
        if (installedVer != null) {
            return JSONObject().apply {
                put("version", installedVer)
                put("source", "installed")
            }
        }

        val catalogVer = ContentManager.CATALOG.firstOrNull { it.type == "Proton" }?.versionName
        if (catalogVer != null) {
            return JSONObject().apply {
                put("version", catalogVer)
                put("source", "catalog default (not confirmed installed)")
            }
        }

        return JSONObject().apply {
            put("version", JSONObject.NULL)
            put("source", "none")
        }
    }

    fun getDxvkVersion(ctx: Context): JSONObject {
        try {
            val containerJson = File(ctx.filesDir, "container/container.json")
            if (containerJson.isFile) {
                val dxvkPath = JSONObject(containerJson.readText()).optString("dxvk", "")
                if (dxvkPath.isNotEmpty()) {
                    val profile = File(dxvkPath, "profile.json")
                    if (profile.isFile) {
                        val ver = JSONObject(profile.readText()).optString("versionName", "")
                        if (ver.isNotEmpty()) {
                            return JSONObject().apply {
                                put("version", ver)
                                put("source", "container")
                            }
                        }
                    }
                    val dirName = File(dxvkPath).name
                    if (dirName.isNotEmpty()) {
                        return JSONObject().apply {
                            put("version", dirName)
                            put("source", "container")
                        }
                    }
                }
            }
        } catch (_: Exception) {}

        if (File(ctx.filesDir, "container/.wine/drive_c/windows/system32/dxgi.dll").isFile) {
            val ver = ContentManager.current(ctx, "DXVK")?.versionName
                ?: ContentManager.list(ctx).firstOrNull { it.type == "DXVK" && ContentManager.isComplete(it) }?.versionName
                ?: ContentManager.list(ctx).firstOrNull { it.type == "DXVK" }?.versionName
            if (ver != null) {
                return JSONObject().apply {
                    put("version", ver)
                    put("source", "container")
                }
            }
        }

        val installedVer = ContentManager.current(ctx, "DXVK")?.versionName
            ?: ContentManager.list(ctx).firstOrNull { it.type == "DXVK" && ContentManager.isComplete(it) }?.versionName
            ?: ContentManager.list(ctx).firstOrNull { it.type == "DXVK" }?.versionName
        if (installedVer != null) {
            return JSONObject().apply {
                put("version", installedVer)
                put("source", "installed")
            }
        }

        val catalogVer = ContentManager.CATALOG.firstOrNull { it.type == "DXVK" }?.versionName
        if (catalogVer != null) {
            return JSONObject().apply {
                put("version", catalogVer)
                put("source", "catalog default (not confirmed installed)")
            }
        }

        return JSONObject().apply {
            put("version", JSONObject.NULL)
            put("source", "none")
        }
    }

    fun currentManifestInfo(
        ctx: Context,
        sc: Shortcut?,
        exePath: String,
        exitCode: Any? = JSONObject.NULL,
        reason: String = "",
        durationSec: Long = 0L,
        gameName: String? = null
    ): JSONObject = try {
        val drivers = DriverManager.getDrivers(ctx)
        val d = sc?.driver?.takeIf { it.isNotEmpty() }?.let { id -> drivers.firstOrNull { it.id == id } }
            ?: DriverManager.getSelectedDriver(ctx, drivers)
        val lib = File(d.libPath)
        val wineVer = getWineVersion(ctx)
        val dxvkVer = getDxvkVersion(ctx)
        val res = if (!sc?.resolution.isNullOrEmpty()) "${DisplayServer.resolution(ctx)} (${sc.resolution})" else DisplayServer.resolution(ctx)

        val actualExitCode = when (exitCode) {
            null, Int.MIN_VALUE -> JSONObject.NULL
            else -> exitCode
        }

        JSONObject().apply {
            put("game", gameName ?: sc?.name ?: if (exePath.isNotEmpty()) File(exePath).name else "")
            put("exe", exePath)
            put("driver", JSONObject().apply {
                put("name", d.name)
                put("id", d.id)
                put("version", d.version)
                put("bundled", d.bundled)
                put("libPath", d.libPath)
                put("soSha256", if (lib.exists() && lib.canRead()) sha256(lib) else JSONObject.NULL)
                put("buildId", elfBuildId(lib) ?: JSONObject.NULL)
            })
            put("wine", JSONObject().apply {
                put("version", wineVer.opt("version"))
                put("source", wineVer.opt("source"))
            })
            put("fexMode", FexPresets.resolve(sc?.fex ?: ""))
            put("dxvk", JSONObject().apply {
                put("enabled", ContainerManager.isDxvkEnabled(ctx))
                put("version", dxvkVer.opt("version"))
                put("source", dxvkVer.opt("source"))
            })
            put("resolution", res)
            put("controller", ControllerInput.config.toJson())
            put("exitCode", actualExitCode)
            put("reason", reason)
            put("durationSec", durationSec)
        }
    } catch (t: Throwable) {
        JSONObject().apply { put("error", t.message ?: t.toString()) }
    }

    private val PROBE_DEV_REGEX = Regex(
        """^(.*?)\s+api=([0-9.]+)\s+driver=(?:0x)?([0-9a-fA-F]+)\s+vendor=(?:0x)?([0-9a-fA-F]+)\s+device=(?:0x)?([0-9a-fA-F]+)"""
    )

    @JvmOverloads
    fun probeGpu(ctx: Context, driverPath: String? = null): JSONObject {
        val probeErrors = JSONArray()
        var firstDeviceLine: String? = null
        var probeSource: String? = null

        val candidateLib = driverPath?.takeIf { it.isNotEmpty() } ?: try {
            val drivers = DriverManager.getDrivers(ctx)
            DriverManager.getSelectedDriver(ctx, drivers).libPath
        } catch (_: Throwable) {
            try {
                DriverManager.bundledDriver(ctx).libPath
            } catch (_: Throwable) {
                null
            }
        }

        if (!candidateLib.isNullOrEmpty()) {
            try {
                val out = Native.probe(candidateLib)
                if (out.startsWith("FAIL")) {
                    probeErrors.put("PanVK driver ($candidateLib): $out")
                } else {
                    val line = out.lineSequence()
                        .map { it.trim() }
                        .firstOrNull { it.isNotEmpty() && !it.startsWith("FAIL") && PROBE_DEV_REGEX.containsMatchIn(it) }
                    if (line != null) {
                        firstDeviceLine = line
                        probeSource = "bundled driver"
                    } else {
                        probeErrors.put("bundled driver ($candidateLib): no device found in output: ${out.take(200)}")
                    }
                }
            } catch (t: Throwable) {
                probeErrors.put("bundled driver ($candidateLib): ${t.message ?: t.toString()}")
            }
        } else {
            probeErrors.put("bundled driver: library path is empty or not found")
        }

        if (firstDeviceLine == null) {
            try {
                val out = Native.probe("libvulkan.so")
                if (out.startsWith("FAIL")) {
                    probeErrors.put("system Vulkan (libvulkan.so): $out")
                } else {
                    val line = out.lineSequence()
                        .map { it.trim() }
                        .firstOrNull { it.isNotEmpty() && !it.startsWith("FAIL") && PROBE_DEV_REGEX.containsMatchIn(it) }
                    if (line != null) {
                        firstDeviceLine = line
                        probeSource = "system Vulkan"
                    } else {
                        probeErrors.put("system Vulkan (libvulkan.so): no device found in output: ${out.take(200)}")
                    }
                }
            } catch (t: Throwable) {
                probeErrors.put("system Vulkan (libvulkan.so): ${t.message ?: t.toString()}")
            }
        }

        val result = JSONObject()
        if (firstDeviceLine != null) {
            val match = PROBE_DEV_REGEX.find(firstDeviceLine)
            if (match != null) {
                val devName = match.groupValues[1].trim()
                val apiVer = match.groupValues[2].trim()
                val vendorId = match.groupValues[4].toLongOrNull(16) ?: 0L
                val deviceId = match.groupValues[5].toLongOrNull(16) ?: 0L

                result.put("deviceName", devName)
                result.put("vendorID", vendorId)
                result.put("deviceID", deviceId)
                result.put("apiVersion", apiVer)

                if (vendorId == 0x13b5L) {
                    val gpuId = "0x%08x".format(deviceId)
                    val arch = "v" + ((deviceId ushr 28) and 0xFL)
                    result.put("gpuId", gpuId)
                    result.put("arch", arch)
                }
                if (probeSource != null) {
                    result.put("probeSource", probeSource)
                }
            }
        }

        if (probeErrors.length() > 0) {
            result.put("probeErrors", probeErrors)
        }
        return result
    }

    fun findGpuModel(ctx: Context, sc: Shortcut? = null, dir: File? = null, probedDevName: String? = null): String {
        if (!probedDevName.isNullOrEmpty()) return probedDevName
        // 1. Search cached vkinfo (JSON / txt in session dir, cacheDir, or filesDir)
        val jsonCandidates = listOfNotNull(
            dir?.let { File(it, "vkinfo.json") },
            File(ctx.cacheDir, "vkinfo.json"),
            File(ctx.filesDir, "vkinfo.json"),
            File(ctx.filesDir, "container/vkinfo.json")
        )
        for (f in jsonCandidates) {
            if (f.isFile && f.canRead()) {
                try {
                    val j = JSONObject(f.readText())
                    val devName = j.optJSONArray("devices")?.optJSONObject(0)
                        ?.optJSONObject("properties")?.optString("deviceName")
                        ?: j.optString("deviceName")
                    if (devName.isNotEmpty()) return devName
                } catch (_: Exception) {}
            }
        }

        val txtCandidates = listOfNotNull(
            dir?.let { File(it, "vkinfo.txt") },
            File(ctx.cacheDir, "vkinfo.txt"),
            File(ctx.filesDir, "vkinfo.txt")
        )
        for (f in txtCandidates) {
            if (f.isFile && f.canRead()) {
                try {
                    val text = f.readText()
                    val match = Regex("""(?i)device\s*name\s*[:=]\s*([^\r\n]+)""").find(text)
                        ?: Regex("""^([A-Za-z0-9_-]+)\s+api=""", RegexOption.MULTILINE).find(text)
                    if (match != null) {
                        val name = match.groupValues[1].trim()
                        if (name.isNotEmpty()) return name
                    }
                } catch (_: Exception) {}
            }
        }

        if (dir != null && dir.isDirectory) {
            val logFiles = dir.listFiles { f -> f.isFile && (f.name.startsWith("dxvk-") || f.name == "wine-run.log") } ?: emptyArray()
            for (f in logFiles) {
                try {
                    val match = Regex("""(?i)Device\s+name:\s*([^\r\n]+)""").find(f.readText())
                    if (match != null) {
                        val name = match.groupValues[1].trim()
                        if (name.isNotEmpty()) return name
                    }
                } catch (_: Exception) {}
            }
        }

        // 2. Search DriverManager (selected or shortcut driver metadata, bundled-driver.json)
        try {
            val drivers = DriverManager.getDrivers(ctx)
            val d = sc?.driver?.takeIf { it.isNotEmpty() }?.let { id -> drivers.firstOrNull { it.id == id } }
                ?: DriverManager.getSelectedDriver(ctx, drivers)

            val driverDir = File(d.libPath).parentFile
            val metaFile = if (driverDir != null) File(driverDir, "meta.json") else null
            if (metaFile != null && metaFile.isFile) {
                try {
                    val j = JSONObject(metaFile.readText())
                    val dev = j.optString("deviceName").ifEmpty { j.optString("gpuModel") }.ifEmpty { j.optString("gpu") }
                    if (dev.isNotEmpty()) return dev
                } catch (_: Exception) {}
            }

            val bundledMeta = try {
                ctx.assets.open("bundled-driver.json").bufferedReader().use { JSONObject(it.readText()) }
            } catch (_: Exception) { null }
            if (bundledMeta != null) {
                val dev = bundledMeta.optString("deviceName").ifEmpty { bundledMeta.optString("gpuModel") }
                if (dev.isNotEmpty()) return dev
            }

            // Driver names describe the driver's target GPU (e.g. "G615"), not this device: never guess from them.
        } catch (_: Exception) {}

        return "hardware: " + Build.HARDWARE
    }

    fun zipDir(ctx: Context) = File(ctx.cacheDir, "session-zips").apply { mkdirs() }

    fun buildCloudZip(ctx: Context, dir: File): File {
        val sessionZipsDir = zipDir(ctx)
        val stageDir = File(sessionZipsDir, "stage-${dir.name}-${System.nanoTime()}").apply {
            deleteRecursively()
            mkdirs()
        }
        val zipFile = File(sessionZipsDir, "${dir.name}-${System.nanoTime()}.zip")

        return try {
            // 1. Copy session dir files under "<dir.name>/"
            val stagedSession = File(stageDir, dir.name).apply { mkdirs() }
            dir.copyRecursively(stagedSession, overwrite = true)

            // 2. system/ extras
            val systemDir = File(stageDir, "system").apply { mkdirs() }

            var gpuinfoRaw: String? = null
            var gpuinfoUnavailableReason: String? = null
            try {
                val f = File("/sys/class/misc/mali0/device/gpuinfo")
                if (!f.exists()) {
                    gpuinfoUnavailableReason = "file missing"
                } else if (!f.canRead()) {
                    gpuinfoUnavailableReason = "not readable (SELinux/permissions)"
                } else {
                    val text = f.readText().trim()
                    if (text.isNotEmpty()) {
                        File(systemDir, "gpuinfo.txt").writeText(text)
                        gpuinfoRaw = text
                    } else {
                        gpuinfoUnavailableReason = "file missing"
                    }
                }
            } catch (e: Exception) {
                gpuinfoUnavailableReason = e.message ?: e.toString()
            }

            var procVersionText: String? = null
            try {
                val f = File("/proc/version")
                if (f.canRead()) {
                    val text = f.readText().trim()
                    if (text.isNotEmpty()) {
                        File(systemDir, "proc_version.txt").writeText(text)
                        procVersionText = text
                    }
                }
            } catch (_: Exception) {}

            try {
                val proc = Runtime.getRuntime().exec(arrayOf("getprop"))
                val lines = proc.inputStream.bufferedReader().use { it.readLines() }
                val keys = listOf("ro.product.", "ro.board.", "ro.soc.", "ro.hardware", "ro.build.version.", "ro.build.fingerprint")
                val filtered = lines.filter { line -> keys.any { line.contains(it) } }
                File(systemDir, "props.txt").writeText(filtered.joinToString("\n"))
            } catch (_: Exception) {}

            // Read manifestInfo from staged session.json
            val sessionJsonFile = File(stagedSession, "session.json")
            val sessionJson = if (sessionJsonFile.isFile) {
                try { JSONObject(sessionJsonFile.readText()) } catch (_: Exception) { null }
            } else null

            val rawManifestInfo = sessionJson?.optJSONObject("manifestInfo")
            val manifestInfoSource: String

            val gameFromSession = sessionJson?.optString("game")?.takeIf { it.isNotEmpty() }
            val exeFromSession = sessionJson?.optString("exe") ?: ""
            val exitFromSession = if (sessionJson != null) {
                if (!sessionJson.isNull("exitCode")) sessionJson.opt("exitCode")
                else if (!sessionJson.isNull("exit")) sessionJson.opt("exit")
                else JSONObject.NULL
            } else JSONObject.NULL
            val reasonFromSession = sessionJson?.optString("reason") ?: ""
            val durFromSession = sessionJson?.optLong("durationSec") ?: 0L

            val allShortcuts = ShortcutStore.list(ctx) + BuiltinTests.list(ctx)
            val matchedShortcut = (if (gameFromSession != null) ShortcutStore.find(ctx, gameFromSession) else null)
                ?: if (exeFromSession.isNotEmpty()) allShortcuts.firstOrNull {
                    it.exe.equals(exeFromSession, true) || ShortcutStore.resolveExe(ctx, it.exe).equals(exeFromSession, true)
                } else null

            val manifestInfo: JSONObject
            if (rawManifestInfo != null) {
                manifestInfoSource = "recorded at session end"
                manifestInfo = rawManifestInfo
            } else {
                manifestInfoSource = "computed at upload time (session predates 1.1.0)"
                manifestInfo = currentManifestInfo(
                    ctx = ctx,
                    sc = matchedShortcut,
                    exePath = exeFromSession,
                    exitCode = exitFromSession,
                    reason = reasonFromSession,
                    durationSec = durFromSession,
                    gameName = gameFromSession
                )
            }

            // 3. Staged files array (excluding manifest.json)
            val filesArray = JSONArray()
            val stagedFiles = stageDir.walkTopDown().filter { it.isFile && it.name != "manifest.json" }
                .sortedBy { it.relativeTo(stageDir).path.replace('\\', '/') }
            for (f in stagedFiles) {
                val relPath = f.relativeTo(stageDir).path.replace('\\', '/')
                filesArray.put(JSONObject().apply {
                    put("path", relPath)
                    put("size", f.length())
                    put("sha256", sha256(f))
                })
            }

            // 4. manifest.json LAST
            val pInfo = try { ctx.packageManager.getPackageInfo(ctx.packageName, 0) } catch (_: Exception) { null }
            val pVersionName = pInfo?.versionName ?: "1.1.0"
            val pVersionCode = if (Build.VERSION.SDK_INT >= 28) (pInfo?.longVersionCode ?: 6L) else @Suppress("DEPRECATION") (pInfo?.versionCode?.toLong() ?: 6L)

            val sessionDriverPath = manifestInfo.optJSONObject("driver")?.optString("libPath")?.takeIf { it.isNotEmpty() && File(it).isFile } // stale after reinstall
            val probedGpu = probeGpu(ctx, sessionDriverPath)
            val probedDevName = probedGpu.optString("deviceName").takeIf { it.isNotEmpty() }
            val gpuModel = findGpuModel(ctx, matchedShortcut, dir, probedDevName)

            val manifestObj = JSONObject().apply {
                put("timestamp", SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()))
                put("app", JSONObject().apply {
                    put("versionName", pVersionName)
                    put("versionCode", pVersionCode)
                })

                // everything from manifestInfo
                for (k in manifestInfo.keys()) {
                    put(k, manifestInfo.get(k))
                }
                put("manifestInfoSource", manifestInfoSource)

                // Keep game/exe/exitCode/reason/durationSec from session.json
                if (sessionJson != null) {
                    if (sessionJson.has("game")) put("game", sessionJson.optString("game"))
                    if (sessionJson.has("exe")) put("exe", sessionJson.optString("exe"))
                    if (sessionJson.has("reason")) put("reason", sessionJson.optString("reason"))
                    if (!sessionJson.isNull("exitCode")) put("exitCode", sessionJson.opt("exitCode"))
                    else if (!sessionJson.isNull("exit")) put("exitCode", sessionJson.opt("exit"))
                    if (sessionJson.has("durationSec")) put("durationSec", sessionJson.optLong("durationSec"))
                }

                put("device", JSONObject().apply {
                    put("manufacturer", Build.MANUFACTURER)
                    put("model", Build.MODEL)
                    put("board", Build.BOARD)
                    put("hardware", Build.HARDWARE)
                    if (Build.VERSION.SDK_INT >= 31) {
                        put("socManufacturer", Build.SOC_MANUFACTURER)
                        put("socModel", Build.SOC_MODEL)
                    } else {
                        put("socManufacturer", JSONObject.NULL)
                        put("socModel", JSONObject.NULL)
                    }
                })

                put("android", JSONObject().apply {
                    put("release", Build.VERSION.RELEASE)
                    put("sdk", Build.VERSION.SDK_INT)
                    put("kernel", procVersionText ?: System.getProperty("os.version") ?: JSONObject.NULL)
                })

                put("gpu", JSONObject().apply {
                    for (k in probedGpu.keys()) {
                        put(k, probedGpu.get(k))
                    }
                    put("gpuinfo", gpuinfoRaw ?: JSONObject.NULL)
                    if (gpuinfoRaw == null) {
                        put("gpuinfo_unavailable_reason", gpuinfoUnavailableReason ?: "file missing")
                    }
                    put("gpuModel", gpuModel)
                })

                put("files", filesArray)
            }

            File(stageDir, "manifest.json").writeText(manifestObj.toString(2))

            // 5. Zip staged files
            zipFiles(zipFile, listOf(Pair("", stageDir)))

            // 6. Verify zip
            verifyZip(zipFile)

            // Delete older zips for the same session (keep the new one)
            sessionZipsDir.listFiles()?.forEach { f ->
                if (f.isFile && f != zipFile && (f.name == "${dir.name}.zip" || f.name.startsWith("${dir.name}-"))) {
                    f.delete()
                }
            }

            zipFile
        } finally {
            stageDir.deleteRecursively()
        }
    }

    /** Zip of one session dir in cache/session-zips (FileProvider path). Uses buildCloudZip so testers get the manifest. */
    fun zip(ctx: Context, dir: File): File = buildCloudZip(ctx, dir)
}
