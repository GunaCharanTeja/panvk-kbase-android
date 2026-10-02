package dev.zenithblue.panvklauncher

import android.content.Context
import android.net.Uri
import android.provider.DocumentsContract
import android.provider.OpenableColumns
import android.system.Os
import android.system.OsConstants
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.nio.file.Files
import java.nio.file.LinkOption
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.TimeUnit

object ContainerManager {

    private const val PREFS_NAME = "launcher"
    private const val KEY_RECENTS = "recent_exes"

    private enum class State {
        IDLE,
        SETTING_UP,
        RUNNING,
        STOPPING
    }

    private val lifecycleLock = Any()
    private var state = State.IDLE
    private var stopRequested = false
    private var currentProcess: Process? = null

    fun isRunning(): Boolean = synchronized(lifecycleLock) {
        state == State.RUNNING || state == State.STOPPING
    }

    fun isSetup(ctx: Context): Boolean {
        val containerDir = File(ctx.filesDir, "container")
        val marker = File(containerDir, ".setup-ok")
        val winePrefix = File(containerDir, ".wine")
        val system32 = File(winePrefix, "drive_c/windows/system32")
        val systemReg = File(winePrefix, "system.reg")
        return marker.isFile && system32.isDirectory && systemReg.isFile
    }

    private fun getContainerConfig(ctx: Context): Pair<File, File?>? {
        val jsonFile = File(ctx.filesDir, "container/container.json")
        if (!jsonFile.isFile) return null
        return try {
            val json = JSONObject(jsonFile.readText())
            val winePath = json.optString("wine", "")
            val fexPath = json.optString("fex", "")
            if (winePath.isNotEmpty()) {
                val wineDir = File(winePath)
                val fexDir = if (fexPath.isNotEmpty()) File(fexPath) else null
                Pair(wineDir, fexDir)
            } else {
                null
            }
        } catch (_: Exception) {
            null
        }
    }

    fun setup(ctx: Context, onLine: (String) -> Unit = {}): Result<Unit> {
        val dir = File(ctx.filesDir, "container")
        val marker = File(dir, ".setup-ok")

        synchronized(lifecycleLock) {
            if (isSetup(ctx)) {
                return Result.success(Unit)
            }
            if (state != State.IDLE) {
                val msg = "Container is busy (${state.name})"
                onLine(msg)
                return Result.failure(IllegalStateException(msg))
            }
            state = State.SETTING_UP
            if (dir.exists() && !marker.isFile) {
                try {
                    ContentManager.deleteTree(dir)
                } catch (t: Throwable) {
                    try {
                        ContentManager.deleteTree(dir)
                    } catch (_: Exception) {}
                    state = State.IDLE
                    return Result.failure(t)
                }
            }
        }

        return try {
            val installed = ContentManager.list(ctx)
            val imagefs = File(ctx.filesDir, "contents/imagefs/bionic")
            if (!imagefs.isDirectory || !installed.any { it.type == "imagefs" && it.versionName == "bionic" }) {
                throw IllegalStateException("imagefs_bionic rootfs is not installed")
            }

            val wine = installed.firstOrNull { it.type == "Proton" }
                ?: throw IllegalStateException("No Proton content installed")
            val fex = installed.firstOrNull { it.type == "FEXCore" }

            if (dir.exists()) {
                ContentManager.deleteTree(dir)
            }
            if (!dir.mkdirs()) {
                throw IOException("Failed to create container directory: ${dir.absolutePath}")
            }

            val prefixPack = File(wine.dir, "prefixPack.txz")
            if (prefixPack.exists()) {
                ContentManager.extractTar(prefixPack, dir, lenient = true)
            }

            val winePrefix = File(dir, ".wine")
            val system32 = File(winePrefix, "drive_c/windows/system32")
            val syswow64 = File(winePrefix, "drive_c/windows/syswow64")
            system32.mkdirs()
            syswow64.mkdirs()

            val dosdevices = File(winePrefix, "dosdevices")
            dosdevices.mkdirs()

            val links = listOf(
                Pair("c:", "../drive_c"),
                Pair("z:", "/"),
                Pair("d:", "/storage/emulated/0/Download"),
                Pair("e:", "/storage/emulated/0")
            )
            for ((name, target) in links) {
                val linkFile = File(dosdevices, name)
                if (!Files.exists(linkFile.toPath(), LinkOption.NOFOLLOW_LINKS) && !Files.isSymbolicLink(linkFile.toPath())) {
                    try {
                        Os.symlink(target, linkFile.absolutePath)
                    } catch (_: Exception) {}
                }
            }

            if (fex != null) {
                val fexProfile = File(fex.dir, "profile.json")
                if (fexProfile.isFile) {
                    val fexJson = JSONObject(fexProfile.readText())
                    val filesArray = fexJson.optJSONArray("files")
                    if (filesArray != null) {
                        val fexCanonical = fex.dir.canonicalPath
                        val prefixCanonical = winePrefix.canonicalPath
                        for (i in 0 until filesArray.length()) {
                            val item = filesArray.optJSONObject(i) ?: continue
                            val sourceRel = item.optString("source", "")
                            val targetRel = item.optString("target", "")

                            val destFile = when {
                                targetRel.startsWith("\${system32}/") -> {
                                    File(system32, targetRel.removePrefix("\${system32}/"))
                                }
                                targetRel.startsWith("\${syswow64}/") -> {
                                    File(syswow64, targetRel.removePrefix("\${syswow64}/"))
                                }
                                else -> null
                            } ?: continue

                            val sourceFile = File(fex.dir, sourceRel)
                            val sourceCanon = sourceFile.canonicalPath
                            val destCanon = destFile.canonicalPath

                            if (sourceCanon != fexCanonical && !sourceCanon.startsWith(fexCanonical + File.separator)) {
                                throw SecurityException("FEX source path escapes fex directory: $sourceRel")
                            }
                            if (destCanon != prefixCanonical && !destCanon.startsWith(prefixCanonical + File.separator)) {
                                throw SecurityException("FEX destination path escapes prefix directory: $targetRel")
                            }

                            if (!sourceFile.exists()) {
                                throw IOException("FEX source file not found: ${sourceFile.absolutePath}")
                            }

                            destFile.parentFile?.mkdirs()
                            sourceFile.copyTo(destFile, overwrite = true)
                            if (sourceFile.canExecute()) {
                                destFile.setExecutable(true, false)
                            }
                        }
                    }
                }
            }

            val containerJson = JSONObject().apply {
                put("wine", wine.dir.absolutePath)
                put("fex", fex?.dir?.absolutePath ?: "")
            }
            File(dir, "container.json").writeText(containerJson.toString(2))

            val exitCode = runInternal(ctx, listOf("wineboot", "-u"), dir, onLine)
            val systemReg = File(winePrefix, "system.reg")
            if (exitCode == 0 && system32.isDirectory && systemReg.isFile) {
                val regArgs = listOf("reg", "add", """HKCU\Software\Wine\Drivers""", "/v", "Graphics", "/d", "null", "/f")
                val regCode = runInternal(ctx, regArgs, dir, onLine)
                if (regCode != 0) {
                    onLine("Wine Graphics=null failed during setup (exit=$regCode)")
                }
                marker.createNewFile()
                Result.success(Unit)
            } else {
                ContentManager.deleteTree(dir)
                Result.failure(IOException("wine wineboot -u failed (exitCode=$exitCode, system32=${system32.isDirectory}, systemReg=${systemReg.isFile})"))
            }
        } catch (t: Throwable) {
            try {
                ContentManager.deleteTree(dir)
            } catch (_: Exception) {}
            Result.failure(t)
        } finally {
            synchronized(lifecycleLock) {
                state = State.IDLE
            }
        }
    }

    fun env(ctx: Context, extra: Map<String, String>? = null): Map<String, String> {
        val containerDir = File(ctx.filesDir, "container")
        containerDir.mkdirs()
        val imagefs = File(ctx.filesDir, "contents/imagefs/bionic")
        val tmpDir = File(imagefs, "usr/tmp")
        tmpDir.mkdirs()

        val config = getContainerConfig(ctx)
        val wineDir = config?.first ?: run {
            val installed = ContentManager.list(ctx)
            installed.firstOrNull { it.type == "Proton" }?.dir ?: File(ctx.filesDir, "contents/Proton/default")
        }

        val selectedDriver = DriverManager.getSelectedDriver(ctx, DriverManager.getDrivers(ctx))
        val icdJson = JSONObject().apply {
            put("file_format_version", "1.0.0")
            put("ICD", JSONObject().apply {
                put("library_path", selectedDriver.libPath)
                put("api_version", "1.4.0")
            })
        }
        val icdFile = File(containerDir, "panvk_icd.json")
        try {
            icdFile.writeText(icdJson.toString(2))
        } catch (_: Exception) {}

        val vklayersDir = File(containerDir, "vklayers")
        vklayersDir.mkdirs()
        val layerSoFile = File(ctx.applicationInfo.nativeLibraryDir, "libVkLayer_fbread.so")
        val layerJsonFile = File(vklayersDir, "VkLayer_panvk_fbread.json")
        val layerJson = JSONObject().apply {
            put("file_format_version", "1.0.0")
            put("layer", JSONObject().apply {
                put("name", "VK_LAYER_panvk_fbread")
                put("type", "GLOBAL")
                put("library_path", layerSoFile.absolutePath)
                put("api_version", "1.3.0")
                put("implementation_version", "1")
                put("description", "present readback")
                put("functions", JSONObject().apply {
                    put("vkGetInstanceProcAddr", "vkGetInstanceProcAddr")
                    put("vkGetDeviceProcAddr", "vkGetDeviceProcAddr")
                })
            })
        }
        try {
            layerJsonFile.writeText(layerJson.toString(2))
        } catch (_: Exception) {}

        val dxvk = isDxvkEnabled(ctx)
        val dllOverrides = if (dxvk) {
            "mscoree,mshtml=d;d3d8,d3d9,d3d10core,d3d11,dxgi=n,b"
        } else {
            "mscoree,mshtml=d"
        }

        val envMap = mutableMapOf(
            "WINEPREFIX" to File(containerDir, ".wine").absolutePath,
            "HOME" to containerDir.absolutePath,
            "TMPDIR" to tmpDir.absolutePath,
            "PATH" to "${wineDir.absolutePath}/bin:${imagefs.absolutePath}/usr/bin:/system/bin",
            "LD_LIBRARY_PATH" to "${imagefs.absolutePath}/usr/lib:/system/lib64:${wineDir.absolutePath}/lib",
            "WINEDLLOVERRIDES" to dllOverrides,
            "WINEDEBUG" to "fixme-all",
            "LC_ALL" to "en_US.UTF-8",
            "FONTCONFIG_PATH" to "${imagefs.absolutePath}/etc/fonts",
            "XDG_DATA_DIRS" to "${imagefs.absolutePath}/usr/share",
            "USER" to "xuser",
            "VK_ICD_FILENAMES" to icdFile.absolutePath,
            "VK_DRIVER_FILES" to icdFile.absolutePath
        )

        if (layerSoFile.exists()) {
            envMap["VK_LAYER_PATH"] = vklayersDir.absolutePath
            envMap["VK_INSTANCE_LAYERS"] = "VK_LAYER_panvk_fbread"
            envMap["PANVK_FB_PATH"] = File(containerDir, "fb.bin").absolutePath
        }

        if (dxvk) {
            envMap["DXVK_LOG_LEVEL"] = "info"
        }

        val fexDll = File(containerDir, ".wine/drive_c/windows/system32/libwow64fex.dll")
        if (fexDll.exists()) {
            envMap["HODLL"] = "libwow64fex.dll"
        }
        envMap.remove("DISPLAY")
        extra?.get("DISPLAY")?.let { envMap["DISPLAY"] = it }
        extra?.get("TMPDIR")?.let { envMap["TMPDIR"] = it }
        extra?.get("XKB_CONFIG_ROOT")?.let { envMap["XKB_CONFIG_ROOT"] = it }
        return envMap
    }

    private fun pruneLogs(logsDir: File) {
        try {
            val files = logsDir.listFiles { f -> f.isFile && f.name.startsWith("run-") && f.name.endsWith(".log") }
            if (files != null && files.size >= 10) {
                val sorted = files.sortedBy { it.name }
                val toDelete = sorted.take(files.size - 9)
                for (f in toDelete) {
                    try { f.delete() } catch (_: Exception) {}
                }
            }
        } catch (_: Exception) {}
    }

    private fun runInternal(
        ctx: Context,
        args: List<String>,
        workingDirectory: File,
        onLine: (String) -> Unit,
        extraEnv: Map<String, String>? = null
    ): Int {
        val config = getContainerConfig(ctx)
        val wineDir = config?.first ?: run {
            val installed = ContentManager.list(ctx)
            installed.firstOrNull { it.type == "Proton" }?.dir
        }
        if (wineDir == null) {
            val msg = "Wine directory not found"
            onLine(msg)
            return -1
        }

        val wineBin = try {
            File(wineDir, "bin/wine").canonicalPath
        } catch (_: Exception) {
            File(wineDir, "bin/wine").absolutePath
        }

        val command = listOf(wineBin) + args

        val logsDir = File(ctx.filesDir, "logs")
        logsDir.mkdirs()
        pruneLogs(logsDir)

        val timestamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        val logFile = File(logsDir, "run-$timestamp.log")

        val cmdStr = "$ " + command.joinToString(" ")
        try { logFile.appendText(cmdStr + "\n") } catch (_: Exception) {}
        onLine(cmdStr)

        val pb = ProcessBuilder(command)
        pb.directory(workingDirectory)
        pb.environment().putAll(env(ctx, extraEnv))
        if (extraEnv == null || !extraEnv.containsKey("DISPLAY")) {
            pb.environment().remove("DISPLAY")
        }
        pb.redirectErrorStream(true)

        val process = try {
            pb.start()
        } catch (e: Exception) {
            val errMsg = e.message ?: e.toString()
            try { logFile.appendText(errMsg + "\n") } catch (_: Exception) {}
            onLine(errMsg)
            return -1
        }

        val aborted = synchronized(lifecycleLock) {
            if (LaunchGate.cancelled(stopRequested, state == State.STOPPING)) {
                true
            } else {
                if (state == State.RUNNING) currentProcess = process
                false
            }
        }
        if (aborted) {
            try {
                process.destroyForcibly()
            } catch (_: Exception) {}
            val cancelMsg = "Process start aborted: stop requested"
            try { logFile.appendText(cancelMsg + "\n") } catch (_: Exception) {}
            onLine(cancelMsg)
            return -1
        }

        return try {
            process.inputStream.bufferedReader().use { reader ->
                var line: String?
                while (reader.readLine().also { line = it } != null) {
                    val l = line!!
                    try { logFile.appendText(l + "\n") } catch (_: Exception) {}
                    onLine(l)
                }
            }
            val exitCode = process.waitFor()
            val exitMsg = "exit=$exitCode"
            try { logFile.appendText(exitMsg + "\n") } catch (_: Exception) {}
            onLine(exitMsg)
            exitCode
        } catch (e: IOException) {
            val errMsg = e.message ?: e.toString()
            try { logFile.appendText(errMsg + "\n") } catch (_: Exception) {}
            onLine(errMsg)
            -1
        } catch (t: Throwable) {
            val errMsg = t.message ?: t.toString()
            try { logFile.appendText(errMsg + "\n") } catch (_: Exception) {}
            onLine(errMsg)
            -1
        } finally {
            synchronized(lifecycleLock) {
                if (currentProcess == process) {
                    currentProcess = null
                }
            }
        }
    }

    fun run(
        ctx: Context,
        args: List<String>,
        workDir: File? = null,
        onLine: (String) -> Unit = {},
        graphics: Boolean = false
    ): Int {
        synchronized(lifecycleLock) {
            if (state == State.SETTING_UP || state == State.STOPPING) {
                val msg = "Container is busy (${state.name})"
                onLine(msg)
                return -1
            }
            if (state == State.RUNNING) {
                val msg = "A process is already running"
                onLine(msg)
                return -1
            }
        }

        if (!isSetup(ctx)) {
            val res = setup(ctx, onLine)
            if (res.isFailure) {
                val msg = "Container setup failed: ${res.exceptionOrNull()?.message}"
                onLine(msg)
                return -1
            }
        }

        synchronized(lifecycleLock) {
            if (state == State.SETTING_UP || state == State.STOPPING) {
                val msg = "Container is busy (${state.name})"
                onLine(msg)
                return -1
            }
            if (state == State.RUNNING) {
                val msg = "A process is already running"
                onLine(msg)
                return -1
            }
            state = State.RUNNING
            stopRequested = false
            currentProcess = null
        }

        val containerDir = File(ctx.filesDir, "container")
        val x11Marker = File(containerDir, ".graphics-x11")
        var session: DisplayServer.Session? = null
        try {
            if (graphics) {
                session = DisplayServer.prepare(ctx, onLine)
                if (session == null || synchronized(lifecycleLock) {
                        LaunchGate.cancelled(stopRequested, state == State.STOPPING)
                    }
                ) {
                    return -1
                }
                if (!x11Marker.isFile) {
                    val regArgs = listOf(
                        "reg", "add", """HKCU\Software\Wine\Drivers""",
                        "/v", "Graphics", "/d", "x11", "/f"
                    )
                    val regCode = runInternal(ctx, regArgs, containerDir, onLine, session.env)
                    if (regCode != 0) {
                        onLine("Wine Graphics=x11 failed (exit=$regCode). Refusing graphical launch.")
                        return -1
                    }
                    try {
                        x11Marker.createNewFile()
                        File(containerDir, ".graphics-null").delete()
                    } catch (_: Exception) {}
                }
                if (synchronized(lifecycleLock) {
                        LaunchGate.cancelled(stopRequested, state == State.STOPPING)
                    }
                ) return -1
            } else if (x11Marker.isFile) {
                val regArgs = listOf(
                    "reg", "add", """HKCU\Software\Wine\Drivers""",
                    "/v", "Graphics", "/d", "null", "/f"
                )
                val regCode = runInternal(ctx, regArgs, containerDir, onLine, null)
                if (regCode == 0) {
                    try { x11Marker.delete() } catch (_: Exception) {}
                } else {
                    onLine("Wine Graphics=null failed (exit=$regCode). Console launch continues.")
                }
            }
            return runInternal(ctx, args, workDir ?: containerDir, onLine, session?.env)
        } finally {
            if (session != null) DisplayServer.close(session)
            synchronized(lifecycleLock) {
                currentProcess = null
                if (state == State.RUNNING || state == State.STOPPING) {
                    state = State.IDLE
                    stopRequested = false
                }
            }
        }
    }

    fun run(ctx: Context, args: List<String>, onLine: (String) -> Unit): Int {
        return run(ctx, args, null, onLine)
    }

    fun runExe(ctx: Context, exePath: String, onLine: (String) -> Unit = {}): Int {
        val exeFile = File(exePath)
        if (!exeFile.isFile) {
            val msg = "File not found: $exePath"
            onLine(msg)
            return -1
        }
        val fbFile = File(ctx.filesDir, "container/fb.bin")
        try { fbFile.delete() } catch (_: Exception) {}
        val workDir = exeFile.parentFile ?: File(ctx.filesDir, "container")
        return run(ctx, listOf(exeFile.absolutePath), workDir = workDir, onLine = onLine, graphics = true)
    }

    fun runExplorer(ctx: Context, onLine: (String) -> Unit = {}): Int {
        return run(ctx, listOf("explorer", "/desktop=shell,1280x720"), onLine = onLine, graphics = true)
    }

    fun stop(ctx: Context) = synchronized(lifecycleLock) {
        if (state != State.RUNNING) {
            return
        }
        state = State.STOPPING
        stopRequested = true
        DisplayServer.stopOwned()

        val proc = currentProcess
        if (proc != null) {
            try {
                proc.destroyForcibly()
            } catch (_: Exception) {}
        }

        try {
            val config = getContainerConfig(ctx)
            val wineDir = config?.first ?: run {
                val installed = ContentManager.list(ctx)
                installed.firstOrNull { it.type == "Proton" }?.dir
            }
            val containerDir = File(ctx.filesDir, "container")

            if (wineDir != null) {
                val wineServer = File(wineDir, "bin/wineserver")
                val wineServerBin = try {
                    wineServer.canonicalPath
                } catch (_: Exception) {
                    wineServer.absolutePath
                }
                val wsCmd = listOf(wineServerBin, "-k")
                try {
                    val pb = ProcessBuilder(wsCmd)
                    pb.directory(containerDir)
                    pb.environment().putAll(env(ctx))
                    pb.environment().remove("DISPLAY")
                    pb.redirectErrorStream(true)
                    val wsProcess = pb.start()
                    val finished = wsProcess.waitFor(10, TimeUnit.SECONDS)
                    if (!finished) {
                        wsProcess.destroyForcibly()
                    }
                } catch (_: Exception) {}
            }

            try {
                val winePrefix = File(ctx.filesDir, "container/.wine")
                val prefixAbs = winePrefix.absolutePath
                val prefixCanon = try { winePrefix.canonicalPath } catch (_: Exception) { prefixAbs }
                val target1 = "WINEPREFIX=$prefixAbs"
                val target2 = "WINEPREFIX=$prefixCanon"

                val myPid = android.os.Process.myPid()
                val procRoot = File("/proc")
                val procList = procRoot.listFiles() ?: emptyArray()
                for (f in procList) {
                    val pid = f.name.toIntOrNull() ?: continue
                    if (pid == myPid) continue
                    try {
                        val environFile = File(f, "environ")
                        if (!environFile.canRead()) continue
                        val bytes = FileInputStream(environFile).use { it.readBytes() }
                        val entries = String(bytes, Charsets.UTF_8).split('\u0000')
                        if (entries.contains(target1) || entries.contains(target2)) {
                            try {
                                Os.kill(pid, OsConstants.SIGKILL)
                            } catch (_: Exception) {}
                        }
                    } catch (_: Exception) {}
                }
            } catch (_: Exception) {}
        } finally {
            currentProcess = null
            // Latch and STOPPING stay until run() returns. Clearing them here
            // lets that owner start Wine after Stop during DisplayServer.prepare.
        }
    }

    fun recentExes(ctx: Context): List<String> {
        val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        val raw = prefs.getString(KEY_RECENTS, null) ?: return emptyList()
        return try {
            val jsonArray = JSONArray(raw)
            val list = mutableListOf<String>()
            for (i in 0 until jsonArray.length()) {
                list.add(jsonArray.getString(i))
            }
            list
        } catch (_: Exception) {
            emptyList()
        }
    }

    fun addRecent(ctx: Context, path: String) {
        val current = recentExes(ctx).toMutableList()
        current.remove(path)
        current.add(0, path)
        val trimmed = if (current.size > 8) current.take(8) else current
        val jsonArray = JSONArray()
        for (item in trimmed) {
            jsonArray.put(item)
        }
        val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        prefs.edit().putString(KEY_RECENTS, jsonArray.toString()).apply()
    }

    fun resolveUriToPath(ctx: Context, uri: Uri): String? {
        if (uri.authority == "com.android.externalstorage.documents") {
            try {
                val docId = DocumentsContract.getDocumentId(uri)
                if (docId.startsWith("primary:")) {
                    val rel = docId.removePrefix("primary:")
                    val hasDotDot = rel.split('/', '\\').any { it == ".." }
                    if (!hasDotDot && !rel.startsWith("/")) {
                        val baseDir = File("/storage/emulated/0")
                        val baseCanon = baseDir.canonicalPath.trimEnd(File.separatorChar) + File.separator
                        val file = File(baseDir, rel)
                        val fileCanon = file.canonicalPath
                        if (fileCanon.startsWith(baseCanon) && file.isFile && file.canRead()) {
                            return file.absolutePath
                        }
                    }
                }
            } catch (_: Exception) {}
        }

        return try {
            var displayName: String? = null
            try {
                ctx.contentResolver.query(
                    uri,
                    arrayOf(OpenableColumns.DISPLAY_NAME),
                    null,
                    null,
                    null
                )?.use { cursor ->
                    if (cursor.moveToFirst()) {
                        val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (idx >= 0) {
                            displayName = cursor.getString(idx)
                        }
                    }
                }
            } catch (_: Exception) {}

            if (displayName.isNullOrEmpty()) {
                displayName = uri.lastPathSegment ?: "import.exe"
            }

            val baseName = File(displayName!!).name
            var sanitized = baseName.filter { it in 'a'..'z' || it in 'A'..'Z' || it in '0'..'9' || it == '.' || it == '_' || it == '-' }
            sanitized = sanitized.replace(Regex("\\.{2,}"), "_")
            while (sanitized.startsWith(".")) {
                sanitized = sanitized.removePrefix(".")
            }
            if (sanitized.isEmpty() || sanitized == "." || sanitized == "..") {
                sanitized = "import.exe"
            }

            val winePrefix = File(ctx.filesDir, "container/.wine")
            val importedRoot = File(winePrefix, "drive_c/imported")
            var ancestorCheck: File? = importedRoot
            while (ancestorCheck != null && ancestorCheck != winePrefix && ancestorCheck.absolutePath.startsWith(winePrefix.absolutePath)) {
                if (ancestorCheck.exists() && Files.isSymbolicLink(ancestorCheck.toPath())) {
                    return null
                }
                ancestorCheck = ancestorCheck.parentFile
            }

            val timestamp = System.currentTimeMillis()
            val destDir = File(winePrefix, "drive_c/imported/$timestamp")

            if (!destDir.mkdirs() && !destDir.isDirectory) {
                return null
            }

            var checkDir: File? = destDir
            while (checkDir != null && checkDir != winePrefix && checkDir.absolutePath.startsWith(winePrefix.absolutePath)) {
                if (Files.isSymbolicLink(checkDir.toPath())) {
                    return null
                }
                checkDir = checkDir.parentFile
            }

            val partFile = File(destDir, "$sanitized.part")
            val destFile = File(destDir, sanitized)

            var writeSuccess = false
            try {
                ctx.contentResolver.openInputStream(uri)?.use { input ->
                    FileOutputStream(partFile).use { output ->
                        input.copyTo(output)
                    }
                    writeSuccess = true
                }
            } catch (_: Exception) {
                writeSuccess = false
            }

            if (!writeSuccess) {
                try { partFile.delete() } catch (_: Exception) {}
                return null
            }

            val renamed = partFile.renameTo(destFile)
            if (!renamed) {
                try { partFile.delete() } catch (_: Exception) {}
                return null
            }

            if (destFile.isFile) {
                destFile.setExecutable(true, false)
                destFile.absolutePath
            } else {
                null
            }
        } catch (_: Exception) {
            null
        }
    }

    fun importUri(ctx: Context, uri: Uri): String? = resolveUriToPath(ctx, uri)

    fun isDxvkEnabled(ctx: Context): Boolean {
        val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getBoolean("dxvk_enabled", false)
    }

    fun setDxvkEnabled(ctx: Context, on: Boolean): String? {
        if (!isSetup(ctx)) {
            return "Container is not set up"
        }
        val prefs = ctx.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        if (on) {
            val installed = ContentManager.list(ctx)
            val dxvk = installed.firstOrNull { it.type == "DXVK" }
                ?: return "DXVK content is not installed"

            val containerDir = File(ctx.filesDir, "container")
            val winePrefix = File(containerDir, ".wine")
            val dstSys32 = File(winePrefix, "drive_c/windows/system32")
            val dstSyswow64 = File(winePrefix, "drive_c/windows/syswow64")
            dstSys32.mkdirs()
            dstSyswow64.mkdirs()

            val dlls = listOf("d3d8.dll", "d3d9.dll", "d3d10core.dll", "d3d11.dll", "dxgi.dll")
            val srcSys32 = File(dxvk.dir, "system32")
            val srcSyswow64 = File(dxvk.dir, "syswow64")

            try {
                for (dll in dlls) {
                    val s32 = File(srcSys32, dll)
                    if (s32.isFile) {
                        s32.copyTo(File(dstSys32, dll), overwrite = true)
                    }
                    val swow64 = File(srcSyswow64, dll)
                    if (swow64.isFile) {
                        swow64.copyTo(File(dstSyswow64, dll), overwrite = true)
                    }
                }
            } catch (e: Exception) {
                return "Failed to copy DXVK DLLs: ${e.message}"
            }
            prefs.edit().putBoolean("dxvk_enabled", true).apply()
        } else {
            prefs.edit().putBoolean("dxvk_enabled", false).apply()
        }
        return null
    }
}

typealias Containers = ContainerManager

