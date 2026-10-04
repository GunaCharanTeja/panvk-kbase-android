package dev.zenithblue.panvktest

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.provider.OpenableColumns
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.DialogProperties
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.concurrent.CancellationException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

data class RunItem(
    val folder: File,
    val name: String,
    val passCount: Int,
    val totalCount: Int
)

enum class DriverType(val label: String) {
    BUNDLED("Bundled PanVK"),
    SYSTEM("System Vulkan"),
    IMPORTED("Imported .so")
}

data class TestCase(
    val name: String,
    val isDraw: Boolean,
    val extraArgsProvider: (Context) -> List<String> = { emptyList() }
)

data class TestResult(
    val name: String,
    val status: String = "IDLE", // IDLE, RUNNING, PASS, FAIL, CRASH, TIMEOUT
    val mismatch: Long = 0,
    val fps: String? = null,
    val durationMs: Long = 0,
    val logFile: File? = null,
    val lastLines: List<String> = emptyList(),
    val isExpanded: Boolean = false,
    val extra: String? = null
)

class MainActivity : ComponentActivity() {

    private val testCases = listOf(
        TestCase("gpu_prerast_slice", isDraw = false),
        TestCase("clip_cull", isDraw = true),
        TestCase("multi_viewport", isDraw = true),
        TestCase("fill_mode", isDraw = true),
        TestCase("bc_decode", isDraw = false) { ctx -> listOf(ctx.cacheDir.absolutePath) },
        TestCase("geometry", isDraw = true),
        TestCase("tessellation", isDraw = true),
        TestCase("xfb", isDraw = true),
        TestCase("pipeline_stats", isDraw = true),
        TestCase("vertex_stores", isDraw = false),
        TestCase("gs_viewport_depth", isDraw = false),
        TestCase("vs_viewport_index", isDraw = false),
        TestCase("depth_bounds", isDraw = false),
        TestCase("large_draw", isDraw = false),
        TestCase("vmr_secondary", isDraw = false),
        TestCase("tess_cond_state", isDraw = false),
        TestCase("swapchain_lifecycle", isDraw = true)
    )

    private var driverTypeState = mutableStateOf(DriverType.BUNDLED)
    private var mesaDebugEnabledState = mutableStateOf(false)
    private var mesaDebugStrState = mutableStateOf("MESA_DEBUG=1 PANVK_DEBUG=trace")
    private var importedFileNameState = mutableStateOf<String?>(null)

    // Info tab state
    private var infoLoadingState = mutableStateOf(false)
    private var infoRawTextState = mutableStateOf<String?>(null)
    private var infoRawJsonState = mutableStateOf<String?>(null)
    private var infoParsedState = mutableStateOf<ParsedVulkanInfo?>(null)

    // Tests tab state
    private var testResultsState = mutableStateOf(
        testCases.map { TestResult(it.name) }
    )
    private var isRunningAllState = mutableStateOf(false)
    @Volatile private var swapSurface: android.view.Surface? = null
    @Volatile private var hungSwapThread: Thread? = null

    // Selected tab: 0=Driver, 1=Info, 2=Tests, 3=Logs
    private var selectedTabState = mutableIntStateOf(0)

    // Logs tab state
    private var selectedLogFileState = mutableStateOf<File?>(null)
    private var selectedLogTextState = mutableStateOf<String?>(null)
    private var logFilesListState = mutableStateOf<List<File>>(emptyList())
    private var runsListState = mutableStateOf<List<RunItem>>(emptyList())
    private val logsSeq = AtomicInteger(0)
    private val runsSeq = AtomicInteger(0)
    private var uploadEndpoint by mutableStateOf(PANVK_UPLOAD_ENDPOINT)

    private fun saveDriverSelection(type: DriverType, importedName: String? = null) {
        val sp = getSharedPreferences("panprobe", Context.MODE_PRIVATE)
        val editor = sp.edit().putString("driver", type.name)
        if (importedName != null) {
            editor.putString("importedName", importedName)
        }
        editor.apply()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val sp = getSharedPreferences("panprobe", Context.MODE_PRIVATE)
        val savedImportedName = sp.getString("importedName", null)
        if (savedImportedName != null) {
            importedFileNameState.value = savedImportedName
        }
        val savedDriver = sp.getString("driver", null)
        if (savedDriver != null) {
            val loadedType = when (savedDriver.uppercase(Locale.US)) {
                "BUNDLED" -> DriverType.BUNDLED
                "SYSTEM" -> DriverType.SYSTEM
                "IMPORTED" -> {
                    val importedSo = File(filesDir, "imported/libimported.so")
                    if (importedSo.exists()) DriverType.IMPORTED else DriverType.BUNDLED
                }
                else -> DriverType.BUNDLED
            }
            driverTypeState.value = loadedType
        }

        // Read intent extras for headless testing
        val driverExtra = intent.getStringExtra("driver")
        if (driverExtra != null) {
            when (driverExtra.lowercase(Locale.US)) {
                "bundled" -> {
                    driverTypeState.value = DriverType.BUNDLED
                    saveDriverSelection(DriverType.BUNDLED)
                }
                "system" -> {
                    driverTypeState.value = DriverType.SYSTEM
                    saveDriverSelection(DriverType.SYSTEM)
                }
                "imported" -> {
                    driverTypeState.value = DriverType.IMPORTED
                    saveDriverSelection(DriverType.IMPORTED)
                }
            }
        }

        val extraEndpoint = intent.getStringExtra("uploadEndpoint")
        uploadEndpoint = if (extraEndpoint != null) {
            val parsed = try { URL(extraEndpoint) } catch (_: Exception) { null }
            val isValid = parsed != null &&
                (parsed.protocol.equals("http", ignoreCase = true) || parsed.protocol.equals("https", ignoreCase = true)) &&
                (parsed.host.equals("127.0.0.1", ignoreCase = true) || parsed.host.equals("localhost", ignoreCase = true))
            if (isValid) extraEndpoint else PANVK_UPLOAD_ENDPOINT
        } else {
            PANVK_UPLOAD_ENDPOINT
        }

        refreshLogsList()
        refreshRunsList()

        val autorunExtra = intent.getStringExtra("autorun")
        // autorun = "all" or a single test name (e.g. gs_viewport_depth)
        if (autorunExtra != null && savedInstanceState == null) {
            selectedTabState.intValue = 2 // Switch UI to Tests tab
            lifecycleScope.launch(Dispatchers.IO) {
                runHeadlessAutorun(autorunExtra)
            }
        }

        setContent {
            PanvkTheme {
                Surface(
                    modifier = Modifier.fillMaxSize().safeDrawingPadding(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    MainScreen()
                }
            }
        }
    }

    private fun refreshLogsList() {
        lifecycleScope.launch(Dispatchers.IO) {
            val seq = logsSeq.incrementAndGet()
            val logsDir = File(filesDir, "logs")
            val files = if (logsDir.exists()) {
                (logsDir.listFiles() ?: emptyArray())
                    .filter { it.isFile }
                    .sortedByDescending { it.lastModified() }
            } else {
                emptyList()
            }
            withContext(Dispatchers.Main) {
                if (seq == logsSeq.get()) {
                    logFilesListState.value = files
                }
            }
        }
    }

    private fun refreshRunsList() {
        lifecycleScope.launch(Dispatchers.IO) {
            val seq = runsSeq.incrementAndGet()
            val runsDir = File(filesDir, "runs")
            val items = if (runsDir.exists()) {
                val folders = (runsDir.listFiles() ?: emptyArray())
                    .filter { it.isDirectory }
                    .sortedByDescending { it.name }
                folders.map { folder ->
                    val summaryFile = File(folder, "summary.json")
                    var pass = 0
                    var total = 0
                    if (summaryFile.exists()) {
                        try {
                            val json = JSONObject(summaryFile.readText())
                            pass = json.optInt("pass", json.optInt("passCount", 0))
                            total = json.optInt("total", 0)
                        } catch (_: Exception) {}
                    }
                    RunItem(folder = folder, name = folder.name, passCount = pass, totalCount = total)
                }
            } else {
                emptyList()
            }
            withContext(Dispatchers.Main) {
                if (seq == runsSeq.get()) {
                    runsListState.value = items
                }
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun getAppVersion(): String {
        return try {
            packageManager.getPackageInfo(packageName, 0).versionName ?: "1.0.0"
        } catch (_: Exception) {
            "1.0.0"
        }
    }

    private suspend fun saveRun(results: List<TestResult>) {
        try { writeRun(results) } catch (e: Exception) { Log.e("PanVKTest", "saveRun failed", e) }
    }

    private suspend fun writeRun(results: List<TestResult>) = withContext(Dispatchers.IO) {
        val timestamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        val runsDir = File(filesDir, "runs").apply { mkdirs() }
        var runFolder = File(runsDir, timestamp)
        var n = 2
        while (runFolder.exists()) runFolder = File(runsDir, "$timestamp-${n++}")
        runFolder.mkdirs()

        for (res in results) {
            val testObj = JSONObject().apply {
                put("name", res.name)
                put("status", res.status)
                put("mismatch", res.mismatch)
                if (res.fps != null) put("fps", res.fps) else put("fps", JSONObject.NULL)
                put("durationMs", res.durationMs)
                if (res.extra != null) put("extra", res.extra) else put("extra", JSONObject.NULL)
                if (res.logFile != null) put("log", res.logFile.name) else put("log", JSONObject.NULL)
                put("lastLines", JSONArray(res.lastLines))
            }
            File(runFolder, "${res.name}.json").writeText(testObj.toString(2))
            res.logFile?.let { srcLog ->
                if (srcLog.exists()) {
                    try {
                        srcLog.copyTo(File(runFolder, srcLog.name), overwrite = true)
                    } catch (_: Exception) {}
                }
            }
        }

        // Always re-query: cached info may belong to a previously selected driver.
        val (rawJsonStr, parsedObj) = run {
            val (obj, raw) = runInfo()
            if (obj != null) {
                val s = obj.toString(2)
                withContext(Dispatchers.Main) {
                    infoRawJsonState.value = s
                    infoParsedState.value = parseVulkanInfo(obj)
                    infoRawTextState.value = null
                }
                Pair(s, obj)
            } else {
                val errObj = JSONObject().apply { put("error", raw) }
                Pair(errObj.toString(2), null)
            }
        }
        File(runFolder, "device_info.json").writeText(rawJsonStr)

        val summaryObj = JSONObject().apply {
            put("timestamp", timestamp)
            put("appVersion", getAppVersion())
            put("driverType", driverTypeState.value.label)

            val firstDevProps = parsedObj?.optJSONArray("devices")?.optJSONObject(0)?.optJSONObject("properties")
            if (firstDevProps != null) {
                for (key in listOf("deviceName", "driverName", "driverInfo", "driverVersion", "apiVersion")) {
                    val v = firstDevProps.opt(key)
                    if (v != null && v != JSONObject.NULL) {
                        put(key, v)
                    }
                }
            }

            val passCount = results.count { it.status == "PASS" }
            put("pass", passCount)
            put("total", results.size)

            val testsObj = JSONObject()
            for (res in results) {
                testsObj.put(res.name, res.status)
            }
            put("tests", testsObj)
        }
        File(runFolder, "summary.json").writeText(summaryObj.toString(2))
    }

    private suspend fun buildRunZip(runFolder: File): File = withContext(Dispatchers.IO) {
        val shareDir = File(cacheDir, "share").apply { mkdirs() }
        val stageDir = File(shareDir, "stage-${runFolder.name}-${System.nanoTime()}").apply {
            deleteRecursively()
            mkdirs()
        }
        val zipFile = File(shareDir, "panprobe-${runFolder.name}.zip")

        try {
            // 1. <run>/ folder
            val stagedRun = File(stageDir, runFolder.name)
            runFolder.copyRecursively(stagedRun, overwrite = true)

            // 2. vulkan-info.json: the run's device_info.json content (or infoRawJsonState if run has none)
            val runDevInfo = File(runFolder, "device_info.json")
            val vkJsonStr = when {
                runDevInfo.exists() -> runDevInfo.readText()
                infoRawJsonState.value != null -> infoRawJsonState.value
                else -> null
            }
            if (vkJsonStr != null) {
                File(stageDir, "vulkan-info.json").writeText(vkJsonStr)
            }

            // 3. logcat.txt (app pid, as now)
            val logcatFile = File(stageDir, "logcat.txt")
            try {
                val process = Runtime.getRuntime().exec(
                    arrayOf("logcat", "-d", "-v", "threadtime", "--pid=" + android.os.Process.myPid())
                )
                val logcatText = process.inputStream.bufferedReader().use { it.readText() }
                logcatFile.writeText(logcatText)
            } catch (_: Exception) {}

            // 4. system/ extras
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
                        gpuinfoUnavailableReason = "not readable (SELinux/permissions)"
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

            var roHardware: String? = null
            try {
                val proc = Runtime.getRuntime().exec(arrayOf("getprop"))
                val lines = proc.inputStream.bufferedReader().use { it.readLines() }
                val keys = listOf("ro.product.", "ro.board.", "ro.soc.", "ro.hardware", "ro.build.version.", "ro.build.fingerprint")
                val filtered = lines.filter { line -> keys.any { line.contains(it) } }
                File(systemDir, "props.txt").writeText(filtered.joinToString("\n"))

                val roHwRegex = Regex("""\[ro\.hardware\]:\s*\[(.*?)\]""")
                for (line in lines) {
                    val m = roHwRegex.find(line)
                    if (m != null) {
                        roHardware = m.groupValues[1].trim().takeIf { it.isNotEmpty() }
                        break
                    }
                }
            } catch (_: Exception) {}

            // 5. Driver resolution: read summary.json driverType if present; fall back to current driverTypeState
            val summaryFile = File(runFolder, "summary.json")
            val summaryDriverTypeLabel = if (summaryFile.exists()) {
                try { JSONObject(summaryFile.readText()).optString("driverType").takeIf { it.isNotEmpty() } } catch (_: Exception) { null }
            } else null

            val resolvedDriverType = DriverType.entries.firstOrNull { it.label == summaryDriverTypeLabel }
                ?: driverTypeState.value
            val driverPath = getDriverPath(resolvedDriverType)
            val driverFile = File(driverPath)
            val driverReadable = driverFile.exists() && driverFile.canRead()
            val driverSoSha256 = if (driverReadable) sha256(driverFile) else null
            val driverBuildId = if (driverReadable) extractGnuBuildId(driverFile) else null

            // 6. Parse vulkan JSON for properties
            val vkJsonObj = try { vkJsonStr?.let { JSONObject(it) } } catch (_: Exception) { null }
            val firstDevProps = vkJsonObj?.optJSONArray("devices")?.optJSONObject(0)?.optJSONObject("properties")
            fun optVal(key: String): Any {
                val v = firstDevProps?.opt(key)
                return if (v == null || v == JSONObject.NULL || v == "null") JSONObject.NULL else v
            }

            // 7. Files array of all staged files (excluding manifest.json)
            val filesArray = JSONArray()
            val stagedFiles = stageDir.walkTopDown().filter { it.isFile }.sortedBy { it.relativeTo(stageDir).path }
            for (f in stagedFiles) {
                val relPath = f.relativeTo(stageDir).path.replace('\\', '/')
                filesArray.put(JSONObject().apply {
                    put("path", relPath)
                    put("size", f.length())
                    put("sha256", sha256(f))
                })
            }

            // 8. manifest.json
            val pInfo = try { packageManager.getPackageInfo(packageName, 0) } catch (_: Exception) { null }
            val manifestObj = JSONObject().apply {
                put("timestamp", SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date()))
                put("app", JSONObject().apply {
                    put("versionName", pInfo?.versionName ?: getAppVersion())
                    put("versionCode", pInfo?.longVersionCode ?: 0L)
                })
                put("driver", JSONObject().apply {
                    put("type", resolvedDriverType.label)
                    put("bundled", resolvedDriverType == DriverType.BUNDLED)
                    put("path", driverPath)
                    put("soSha256", driverSoSha256 ?: JSONObject.NULL)
                    put("buildId", driverBuildId ?: JSONObject.NULL)
                    put("driverName", optVal("driverName"))
                    put("driverInfo", optVal("driverInfo"))
                    put("driverVersion", optVal("driverVersion"))
                })
                put("device", JSONObject().apply {
                    put("manufacturer", android.os.Build.MANUFACTURER)
                    put("model", android.os.Build.MODEL)
                    put("board", android.os.Build.BOARD)
                    put("hardware", android.os.Build.HARDWARE)
                    if (android.os.Build.VERSION.SDK_INT >= 31) {
                        put("socManufacturer", android.os.Build.SOC_MANUFACTURER)
                        put("socModel", android.os.Build.SOC_MODEL)
                    } else {
                        put("socManufacturer", JSONObject.NULL)
                        put("socModel", JSONObject.NULL)
                    }
                })
                put("gpu", JSONObject().apply {
                    val vkDeviceName = (optVal("deviceName") as? String)?.takeIf { it.isNotBlank() && it != "null" }
                    val gpuinfoModel = if (gpuinfoRaw != null) Regex("""Mali-[A-Za-z0-9]+""").find(gpuinfoRaw)?.value else null
                    val hwFallback = (android.os.Build.HARDWARE.takeIf { it.isNotBlank() && !it.equals("unknown", ignoreCase = true) } ?: roHardware)?.let { "hardware: $it" }
                    val gpuModel = vkDeviceName ?: gpuinfoModel ?: hwFallback

                    put("deviceName", optVal("deviceName"))
                    put("deviceID", optVal("deviceID"))
                    put("vendorID", optVal("vendorID"))
                    put("apiVersion", optVal("apiVersion"))
                    put("gpuinfo", gpuinfoRaw ?: JSONObject.NULL)
                    if (gpuinfoRaw == null) {
                        put("gpuinfo_unavailable_reason", gpuinfoUnavailableReason ?: "not readable (SELinux/permissions)")
                    }
                    // gpuinfo sysfs is usually SELinux-blocked; fall back to the Vulkan deviceID,
                    // which is the Mali gpu_id on ARM (vendor 0x13B5). Arch major = gpu_id[31:28].
                    val vkId = (optVal("deviceID") as? Number)?.toLong()
                    val isArm = (optVal("vendorID") as? Number)?.toLong() == 0x13B5L
                    val gpuId = (if (gpuinfoRaw != null) Regex("""0x[0-9a-fA-F]+""").find(gpuinfoRaw)?.value else null)
                        ?: if (isArm && vkId != null) "0x%08x".format(vkId) else null
                    put("gpuId", gpuId ?: JSONObject.NULL)
                    val archMajor = gpuId?.removePrefix("0x")?.toLongOrNull(16)?.let { (it ushr 28) and 0xF }
                    put("arch", archMajor?.let { "v$it" } ?: JSONObject.NULL)
                    put("gpuModel", gpuModel ?: JSONObject.NULL)
                })
                put("android", JSONObject().apply {
                    put("release", android.os.Build.VERSION.RELEASE)
                    put("sdk", android.os.Build.VERSION.SDK_INT)
                    put("kernel", procVersionText ?: System.getProperty("os.version") ?: JSONObject.NULL)
                })
                put("files", filesArray)
            }
            File(stageDir, "manifest.json").writeText(manifestObj.toString(2))

            // 9. Zip staged files
            zipFiles(zipFile, listOf(Pair("", stageDir)))

            // 10. Self-check
            verifyZip(zipFile)

            zipFile
        } finally {
            stageDir.deleteRecursively()
        }
    }

    private fun getDriverPath(type: DriverType): String {
        return when (type) {
            DriverType.BUNDLED -> File(applicationInfo.nativeLibraryDir, "libvulkan_panfrost.so").absolutePath
            DriverType.SYSTEM -> "libvulkan.so"
            DriverType.IMPORTED -> File(filesDir, "imported/libimported.so").absolutePath
        }
    }

    private fun buildEnv(isDraw: Boolean): Array<String> {
        val envList = mutableListOf<String>()
        envList.add("MESA_LOG=file")
        envList.add("TMPDIR=${cacheDir.absolutePath}")
        if (mesaDebugEnabledState.value) {
            mesaDebugStrState.value.trim().split(Regex("\\s+"))
                .filter { it.isNotEmpty() && it.contains("=") }
                .forEach { envList.add(it) }
        }
        if (isDraw) {
            envList.add("PT_FPS_MS=1000")
        }
        return envList.toTypedArray()
    }

    private fun createLogFile(name: String): File {
        val logsDir = File(filesDir, "logs").apply { mkdirs() }
        val timestamp = SimpleDateFormat("yyyyMMdd-HHmmss", Locale.US).format(Date())
        return File(logsDir, "$timestamp-$name.log")
    }

    private suspend fun runInfo(): Pair<JSONObject?, String> = withContext(Dispatchers.IO) {
        val logFile = createLogFile("vkinfo")
        val libPath = File(applicationInfo.nativeLibraryDir, "libt_vkinfo.so").absolutePath
        val driverPath = getDriverPath(driverTypeState.value)
        val env = buildEnv(isDraw = false)
        val jsonFile = File(cacheDir, "vkinfo.json").apply { delete() }
        val res = Native.run(libPath, arrayOf(driverPath, jsonFile.absolutePath), env, logFile.absolutePath, 30000)

        val logContent = if (logFile.exists()) logFile.readText() else ""
        refreshLogsList()

        val jsonText = if (jsonFile.exists()) jsonFile.readText() else ""
        val jsonObject = try { JSONObject(jsonText) } catch (_: Exception) { null }

        Pair(jsonObject, if (jsonObject != null) jsonText else "Result: $res\n$logContent")
    }


    private suspend fun executeTest(test: TestCase): TestResult = withContext(Dispatchers.IO) {
        if (test.name == "swapchain_lifecycle") return@withContext executeSwapchainTest()
        val t0 = System.currentTimeMillis()
        val logFile = createLogFile(test.name)
        val libPath = File(applicationInfo.nativeLibraryDir, "libt_${test.name}.so").absolutePath
        val driverPath = getDriverPath(driverTypeState.value)
        val args = (listOf(driverPath) + test.extraArgsProvider(this@MainActivity)).toTypedArray()
        val env = buildEnv(test.isDraw)

        val res = Native.run(libPath, args, env, logFile.absolutePath, 60000)
        val durationMs = System.currentTimeMillis() - t0

        val logContent = if (logFile.exists()) logFile.readText() else ""
        val lines = logContent.lines()
        val hasFail = lines.any { it.trimStart().startsWith("FAIL") }

        val status = when {
            res == "timeout" -> "TIMEOUT"
            res.startsWith("signal:") -> {
                try {
                    val crashFile = File(logFile.parentFile, "${logFile.nameWithoutExtension}.crash")
                    logFile.copyTo(crashFile, overwrite = true)
                } catch (_: Exception) {}
                "CRASH"
            }
            res == "exit:0" && !hasFail -> "PASS"
            else -> "FAIL"
        }

        val mismatchSum = Regex("""mismatch=(\d+)""").findAll(logContent)
            .sumOf { it.groupValues[1].toLongOrNull() ?: 0L }
        val fps = Regex("""FPS ([0-9.]+)""").find(logContent)?.groupValues?.get(1)
        val last40 = lines.takeLast(40)

        refreshLogsList()

        TestResult(
            name = test.name,
            status = status,
            mismatch = mismatchSum,
            fps = fps,
            durationMs = durationMs,
            logFile = logFile,
            lastLines = last40
        )
    }

    private suspend fun executeSwapchainTest(): TestResult = withContext(Dispatchers.IO) {
        val t0 = System.currentTimeMillis()
        if (hungSwapThread?.isAlive == true) {
            return@withContext TestResult(
                name = "swapchain_lifecycle",
                status = "FAIL",
                durationMs = System.currentTimeMillis() - t0,
                lastLines = listOf("FAIL previous swapchain run still hung")
            )
        }

        val surfaceWaitEnd = System.currentTimeMillis() + 5_000L
        var surface = swapSurface
        while (surface == null && System.currentTimeMillis() < surfaceWaitEnd) {
            Thread.sleep(100)
            surface = swapSurface
        }
        if (surface == null) {
            return@withContext TestResult(
                name = "swapchain_lifecycle",
                status = "FAIL",
                durationMs = System.currentTimeMillis() - t0,
                lastLines = listOf("FAIL no surface")
            )
        }
        val boundSurface = surface

        val logFile = createLogFile("swapchain_lifecycle")
        val holder = object {
            @Volatile var result: String? = null
        }
        val thread = Thread({
            holder.result = Native.swapchainTest(
                getDriverPath(driverTypeState.value),
                boundSurface,
                logFile.absolutePath
            )
        }, "swapchain-test")
        thread.isDaemon = true
        thread.start()

        // g_done stays 1 until this run calls set_done(0). Ignore that stale sample.
        var sawRun = false
        var hangPhase: String? = null
        val runDeadline = System.currentTimeMillis() + 120_000L
        while (true) {
            Thread.sleep(100)
            val parts = Native.swapchainPhase().split(' ')
            val phase = parts.getOrElse(0) { "" }
            val ms = parts.getOrNull(1)?.toLongOrNull() ?: 0L
            val done = parts.getOrNull(2) == "1"
            if (!done) sawRun = true
            if (!thread.isAlive || (done && sawRun)) break
            if (System.currentTimeMillis() >= runDeadline || (sawRun && ms > 10_000L)) {
                logFile.appendText("FAIL hang in $phase (watchdog 10s)\n")
                hungSwapThread = thread
                hangPhase = phase
                break
            }
        }
        if (hangPhase == null) thread.join()

        val result = holder.result ?: ""
        val logContent = if (logFile.exists()) logFile.readText() else ""
        val lines = logContent.lines()
        val hasFail = lines.any { it.trimStart().startsWith("FAIL") }
        val status = if (hangPhase == null && result == "exit:0" && !hasFail) "PASS" else "FAIL"
        val fps = Regex("""FPS ([0-9.]+)""").find(logContent)?.groupValues?.get(1)
        val resized = Regex("""FPS_RESIZED ([0-9.]+)""").find(logContent)?.groupValues?.get(1) ?: ""
        val phases = Regex("""PHASE (\S+) ms=(\d+)""")
            .findAll(logContent)
            .joinToString(", ") { "${it.groupValues[1]} ${it.groupValues[2]}ms" }
        val extra = (if (hangPhase != null) "hang in $hangPhase | " else "") +
            "resized FPS $resized | $phases"

        refreshLogsList()
        TestResult(
            name = "swapchain_lifecycle",
            status = status,
            fps = fps,
            durationMs = System.currentTimeMillis() - t0,
            logFile = logFile,
            lastLines = lines.takeLast(40),
            extra = extra
        )
    }

    // Autorun summary: logcat + files/autorun.txt (some devices drop app logcat over adb).
    private fun say(s: String) {
        Log.i("PanVKTest", s)
        java.io.File(filesDir, "autorun.txt").appendText(s + "\n")
    }

    private suspend fun runHeadlessAutorun(which: String) {
        java.io.File(filesDir, "autorun.txt").delete()
        val (json, _) = runInfo()
        val devicesArr = json?.optJSONArray("devices")
        val numDevices = devicesArr?.length() ?: 0
        val firstDev = if (numDevices > 0) devicesArr?.optJSONObject(0) else null
        val numExts = firstDev?.optJSONArray("extensions")?.length()
            ?: (json?.optJSONArray("instanceExtensions")?.length() ?: 0)
        val numFormats = firstDev?.optJSONObject("formats")?.length() ?: 0

        say("INFO devices=$numDevices exts=$numExts formats=$numFormats")

        var passCount = 0
        val selected = if (which == "all") testCases else testCases.filter { it.name == which }
        val runResults = mutableListOf<TestResult>()
        for (test in selected) {
            withContext(Dispatchers.Main) {
                updateTestStatus(test.name, "RUNNING")
            }
            val res = executeTest(test)
            runResults.add(res)
            withContext(Dispatchers.Main) {
                updateTestResult(res)
            }
            say("RESULT ${test.name} ${res.status} mismatch=${res.mismatch} fps=${res.fps ?: "0"} ms=${res.durationMs}${if (res.extra != null) " extra=${res.extra}" else ""}")
            if (res.status == "PASS") {
                passCount++
            }
        }
        say("AUTORUN DONE pass=$passCount total=${selected.size}")
        saveRun(runResults)
        withContext(Dispatchers.Main) {
            refreshRunsList()
        }
    }

    private fun updateTestStatus(testName: String, status: String) {
        testResultsState.value = testResultsState.value.map {
            if (it.name == testName) it.copy(status = status) else it
        }
    }

    private fun updateTestResult(res: TestResult) {
        testResultsState.value = testResultsState.value.map {
            if (it.name == res.name) res else it
        }
    }

    private fun startRunAll() {
        if (isRunningAllState.value) return
        lifecycleScope.launch(Dispatchers.Main) {
            isRunningAllState.value = true
            val runResults = mutableListOf<TestResult>()
            for (test in testCases) {
                updateTestStatus(test.name, "RUNNING")
                val res = executeTest(test)
                runResults.add(res)
                updateTestResult(res)
            }
            saveRun(runResults)
            refreshRunsList()
            isRunningAllState.value = false
        }
    }

    @Composable
    fun MainScreen() {
        val currentTab = AppTab.entries.getOrElse(selectedTabState.intValue) { AppTab.Driver }
        val isAnyRunning = isRunningAllState.value || infoLoadingState.value || testResultsState.value.any { it.status == "RUNNING" }

        AppShell(
            tab = currentTab,
            onTab = { selectedTabState.intValue = it.ordinal },
            running = isAnyRunning
        ) { tab ->
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .padding(horizontal = 16.dp, vertical = 8.dp),
                contentAlignment = Alignment.TopCenter
            ) {
                Box(
                    modifier = Modifier
                        .widthIn(max = 760.dp)
                        .fillMaxSize()
                ) {
                    when (tab) {
                        AppTab.Driver -> DriverTab()
                        AppTab.Info -> InfoTab()
                        AppTab.Tests -> TestsTab()
                        AppTab.Logs -> LogsTab()
                    }
                }
            }
        }
    }

    @Composable
    fun DriverTab() {
        val context = LocalContext.current
        val pickLauncher = rememberLauncherForActivityResult(
            contract = ActivityResultContracts.OpenDocument()
        ) { uri: Uri? ->
            if (uri != null) {
                try {
                    val importedDir = File(context.filesDir, "imported").apply { mkdirs() }
                    val destFile = File(importedDir, "libimported.so")
                    context.contentResolver.openInputStream(uri)?.use { input ->
                        destFile.outputStream().use { output ->
                            input.copyTo(output)
                        }
                    }
                    var name = uri.lastPathSegment ?: "libimported.so"
                    context.contentResolver.query(uri, null, null, null, null)?.use { cursor ->
                        val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                        if (idx != -1 && cursor.moveToFirst()) {
                            name = cursor.getString(idx)
                        }
                    }
                    importedFileNameState.value = name
                    saveDriverSelection(driverTypeState.value, name)
                } catch (e: Exception) {
                    importedFileNameState.value = "Error: ${e.message}"
                }
            }
        }

        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            SectionTitle("Vulkan Driver Selection")

            DriverType.entries.forEach { type ->
                val isSelected = driverTypeState.value == type
                OutlinedCard(
                    onClick = {
                        driverTypeState.value = type
                        saveDriverSelection(type)
                    },
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.outlinedCardColors(
                        containerColor = if (isSelected) MaterialTheme.colorScheme.surfaceContainerHigh else MaterialTheme.colorScheme.surface
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        RadioButton(
                            selected = isSelected,
                            onClick = {
                                driverTypeState.value = type
                                saveDriverSelection(type)
                            }
                        )
                        Spacer(Modifier.width(8.dp))
                        Column(Modifier.weight(1f)) {
                            Text(type.label, style = MaterialTheme.typography.bodyLarge, fontWeight = FontWeight.SemiBold)
                            Text(
                                text = getDriverPath(type),
                                style = MaterialTheme.typography.bodySmall,
                                fontFamily = FontFamily.Monospace,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }

            if (driverTypeState.value == DriverType.IMPORTED) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)
                ) {
                    Column(
                        modifier = Modifier.padding(14.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(onClick = { pickLauncher.launch(arrayOf("*/*")) }) {
                            Text("Select .so file")
                        }
                        importedFileNameState.value?.let {
                            Text("Imported file: $it", style = MaterialTheme.typography.bodyMedium)
                        }
                    }
                }
            }

            HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)

            SectionTitle("Mesa Environment")

            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)
            ) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(10.dp)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clickable { mesaDebugEnabledState.value = !mesaDebugEnabledState.value },
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Checkbox(
                            checked = mesaDebugEnabledState.value,
                            onCheckedChange = { mesaDebugEnabledState.value = it }
                        )
                        Spacer(Modifier.width(8.dp))
                        Text("Mesa debug env", style = MaterialTheme.typography.bodyMedium, fontWeight = FontWeight.Medium)
                    }

                    OutlinedTextField(
                        value = mesaDebugStrState.value,
                        onValueChange = { mesaDebugStrState.value = it },
                        label = { Text("Debug Variables (space-separated)") },
                        modifier = Modifier.fillMaxWidth()
                    )
                }
            }

            Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)) {
                Column(
                    modifier = Modifier.padding(14.dp),
                    verticalArrangement = Arrangement.spacedBy(4.dp)
                ) {
                    Text("Always passed:", fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                    Text("• MESA_LOG=file", style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
                    Text("• TMPDIR=${context.cacheDir.absolutePath}", style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace)
                }
            }
        }
    }

    @Composable
    fun InfoTab() {
        val coroutineScope = rememberCoroutineScope()
        InfoTabContent(
            isLoading = infoLoadingState.value,
            parsedInfo = infoParsedState.value,
            rawJson = infoRawJsonState.value,
            rawErrorText = infoRawTextState.value,
            hasRuns = {
                val runsDir = File(filesDir, "runs")
                runsDir.exists() && (runsDir.listFiles()?.any { it.isDirectory } == true)
            },
            onRunTests = {
                selectedTabState.intValue = 2
                startRunAll()
            },
            onLoadClick = {
                coroutineScope.launch {
                    infoLoadingState.value = true
                    val (json, raw) = runInfo()
                    if (json != null) {
                        infoParsedState.value = parseVulkanInfo(json)
                        infoRawJsonState.value = json.toString(2)
                        infoRawTextState.value = null
                    } else {
                        infoParsedState.value = null
                        infoRawJsonState.value = null
                        infoRawTextState.value = raw
                    }
                    infoLoadingState.value = false
                }
            }
        )
    }

    @Composable
    fun TestsTab() {
        val coroutineScope = rememberCoroutineScope()

        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                SectionTitle("Tests")
                Button(
                    enabled = !isRunningAllState.value,
                    onClick = { startRunAll() }
                ) {
                    Text(if (isRunningAllState.value) "Running..." else "Run all")
                }
            }

            Spacer(Modifier.height(8.dp))

            if (isRunningAllState.value) {
                BusyCard("Running test suite...", modifier = Modifier.padding(bottom = 8.dp))
            }

            Surface(
                modifier = Modifier
                    .fillMaxWidth()
                    .height(160.dp),
                shape = MaterialTheme.shapes.medium,
                color = Color.Black
            ) {
                AndroidView(
                    factory = { ctx ->
                        SurfaceView(ctx).apply {
                            holder.addCallback(object : SurfaceHolder.Callback {
                                override fun surfaceCreated(holder: SurfaceHolder) {
                                    swapSurface = holder.surface
                                }
                                override fun surfaceChanged(
                                    holder: SurfaceHolder, format: Int, width: Int, height: Int
                                ) {
                                    swapSurface = holder.surface
                                }
                                override fun surfaceDestroyed(holder: SurfaceHolder) {
                                    swapSurface = null
                                }
                            })
                        }
                    },
                    modifier = Modifier.fillMaxSize()
                )
            }

            Spacer(Modifier.height(8.dp))

            LazyColumn(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                contentPadding = PaddingValues(bottom = 16.dp)
            ) {
                items(testCases) { test ->
                    val result = testResultsState.value.firstOrNull { it.name == test.name } ?: TestResult(test.name)
                    TestRow(
                        test = test,
                        result = result,
                        onRunClick = {
                            coroutineScope.launch {
                                updateTestStatus(test.name, "RUNNING")
                                val res = executeTest(test)
                                updateTestResult(res)
                                saveRun(listOf(res))
                                refreshRunsList()
                            }
                        },
                        onToggleExpand = {
                            testResultsState.value = testResultsState.value.map {
                                if (it.name == test.name) it.copy(isExpanded = !it.isExpanded) else it
                            }
                        }
                    )
                }
            }
        }
    }

    @Composable
    fun TestRow(
        test: TestCase,
        result: TestResult,
        onRunClick: () -> Unit,
        onToggleExpand: () -> Unit
    ) {
        Card(
            modifier = Modifier
                .fillMaxWidth()
                .clickable { onToggleExpand() },
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)
        ) {
            Column(modifier = Modifier.padding(12.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(test.name, fontWeight = FontWeight.Bold, style = MaterialTheme.typography.bodyLarge)
                            if (test.isDraw) {
                                Spacer(Modifier.width(6.dp))
                                StatusPill(
                                    text = "draw",
                                    tone = Tone.Accent
                                )
                            }
                        }
                        Spacer(Modifier.height(4.dp))
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            StatusPill(
                                text = result.status,
                                tone = testStatusTone(result.status)
                            )
                            if (result.status != "IDLE" && result.status != "RUNNING") {
                                Text("mismatch=${result.mismatch}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                if (result.fps != null) {
                                    Text("FPS ${result.fps}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                }
                                Text("${result.durationMs}ms", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                        }
                        result.extra?.let {
                            Spacer(Modifier.height(2.dp))
                            Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                    }

                    Spacer(Modifier.width(8.dp))

                    Button(
                        onClick = onRunClick,
                        enabled = result.status != "RUNNING" && !isRunningAllState.value,
                        contentPadding = PaddingValues(horizontal = 16.dp, vertical = 6.dp)
                    ) {
                        Text("Run")
                    }
                }

                if (result.isExpanded && result.lastLines.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    HorizontalDivider(color = MaterialTheme.colorScheme.outlineVariant)
                    Spacer(Modifier.height(6.dp))
                    Text(
                        "Last ${result.lastLines.size} lines:",
                        style = MaterialTheme.typography.labelSmall,
                        color = MaterialTheme.colorScheme.primary,
                        fontWeight = FontWeight.SemiBold
                    )
                    Spacer(Modifier.height(4.dp))
                    Surface(
                        modifier = Modifier.fillMaxWidth(),
                        shape = MaterialTheme.shapes.small,
                        color = MaterialTheme.colorScheme.surfaceContainerLowest
                    ) {
                        SelectionContainer(modifier = Modifier.padding(8.dp)) {
                            Text(
                                text = result.lastLines.joinToString("\n"),
                                fontFamily = FontFamily.Monospace,
                                fontSize = 11.sp,
                                lineHeight = 14.sp
                            )
                        }
                    }
                }
            }
        }
    }

    @Composable
    fun LogsTab() {
        val context = LocalContext.current
        val coroutineScope = rememberCoroutineScope()

        var cloudConfirmRun by remember { mutableStateOf<RunItem?>(null) }
        var isUploading by remember { mutableStateOf(false) }
        var showLinkDialog by remember { mutableStateOf(false) }
        var showErrorDialog by remember { mutableStateOf(false) }
        var uploadZipFile by remember { mutableStateOf<File?>(null) }
        var uploadSha256 by remember { mutableStateOf<String?>(null) }
        var pathAState by remember { mutableStateOf(UploadPathState(name = "catbox / gofile")) }
        var pathBState by remember { mutableStateOf(UploadPathState(name = "PanVK storage (R2)")) }
        var isRetryingA by remember { mutableStateOf(false) }
        var isRetryingB by remember { mutableStateOf(false) }
        val currentCancelFlag = remember { mutableStateOf(AtomicBoolean(false)) }
        val uploadGeneration = remember { AtomicInteger(0) }

        DisposableEffect(Unit) {
            onDispose {
                currentCancelFlag.value.set(true)
                uploadGeneration.incrementAndGet()
            }
        }

        fun startDualUpload(
            zip: File,
            localSha: String,
            existingGen: Int? = null,
            existingFlag: AtomicBoolean? = null
        ) {
            val gen = existingGen ?: uploadGeneration.incrementAndGet()
            val flag = existingFlag ?: AtomicBoolean(false).also {
                currentCancelFlag.value.set(true)
                currentCancelFlag.value = it
            }
            uploadZipFile = zip
            uploadSha256 = localSha
            coroutineScope.launch {
                if (flag.get() || uploadGeneration.get() != gen) return@launch
                val endpoint = uploadEndpoint
                val bInitialStatus = if (endpoint.isEmpty()) "Skipped (not configured)" else "Uploading"
                val bInitialError = if (endpoint.isEmpty()) "Skipped (not configured)" else null
                pathAState = UploadPathState(name = "catbox / gofile", status = "Uploading")
                pathBState = UploadPathState(name = "PanVK storage (R2)", status = bInitialStatus, error = bInitialError)
                isUploading = true
                showLinkDialog = false
                showErrorDialog = false

                try {
                    pathAState = pathAState.copy(totalBytes = zip.length())
                    if (endpoint.isNotEmpty()) {
                        pathBState = pathBState.copy(totalBytes = zip.length())
                    }

                    coroutineScope {
                        val jobA = async(Dispatchers.IO) {
                            var loggedCancelA = false
                            fun logCancel() {
                                if (!loggedCancelA) {
                                    loggedCancelA = true
                                    Log.i("PanProbe", "upload cancelled")
                                }
                            }

                            if (flag.get() || uploadGeneration.get() != gen) {
                                if (flag.get()) logCancel()
                                return@async
                            }
                            var lastPercentA = -1
                            try {
                                val resA = uploadToCloud(zip, getAppVersion(), flag) { sent, total ->
                                    val pct = if (total > 0) ((sent * 100) / total).toInt() else 0
                                    if (pct != lastPercentA || sent == total) {
                                        lastPercentA = pct
                                        if (!flag.get() && uploadGeneration.get() == gen) {
                                            pathAState = pathAState.copy(bytesSent = sent, totalBytes = total)
                                        }
                                    }
                                }
                                if (flag.get() || uploadGeneration.get() != gen) {
                                    if (flag.get()) logCancel()
                                    return@async
                                }
                                if (resA.directUrl == null) {
                                    if (uploadGeneration.get() == gen) {
                                        pathAState = pathAState.copy(
                                            url = resA.url,
                                            directUrl = null,
                                            status = "Done (not verified, gofile)",
                                            verifyStatus = "– not verified",
                                            bytesSent = zip.length(),
                                            totalBytes = zip.length()
                                        )
                                    }
                                } else {
                                    if (uploadGeneration.get() == gen) {
                                        pathAState = pathAState.copy(
                                            url = resA.url,
                                            directUrl = resA.directUrl,
                                            status = "Verifying",
                                            bytesSent = zip.length(),
                                            totalBytes = zip.length()
                                        )
                                    }
                                    val vA = verifyUpload(resA.directUrl, localSha, getAppVersion(), flag)
                                    if (flag.get() || uploadGeneration.get() != gen) {
                                        if (flag.get()) logCancel()
                                        return@async
                                    }
                                    if (uploadGeneration.get() == gen) {
                                        if (vA == "Verified ✓") {
                                            pathAState = pathAState.copy(status = "Done ✓ verified", verifyStatus = "✓")
                                        } else {
                                            pathAState = pathAState.copy(status = "Failed: $vA", verifyStatus = "✗")
                                        }
                                    }
                                }
                            } catch (e: CancellationException) {
                                logCancel()
                                throw e
                            } catch (e: Exception) {
                                if (flag.get()) {
                                    logCancel()
                                    throw CancellationException("Upload cancelled")
                                }
                                if (!flag.get() && uploadGeneration.get() == gen) {
                                    val msg = friendlyUploadError(e)
                                    pathAState = pathAState.copy(status = "Failed: $msg", error = msg)
                                }
                            }
                        }

                        val jobB = async(Dispatchers.IO) {
                            var loggedCancelB = false
                            fun logCancel() {
                                if (!loggedCancelB) {
                                    loggedCancelB = true
                                    Log.i("PanProbe", "upload cancelled")
                                }
                            }

                            if (endpoint.isEmpty()) {
                                if (uploadGeneration.get() == gen) {
                                    pathBState = pathBState.copy(status = "Skipped (not configured)", error = "Skipped (not configured)")
                                }
                                return@async
                            }

                            if (flag.get() || uploadGeneration.get() != gen) {
                                if (flag.get()) logCancel()
                                return@async
                            }
                            var lastPercentB = -1
                            try {
                                val resB = uploadToR2(
                                    endpoint = endpoint,
                                    f = zip,
                                    sha256Hex = localSha,
                                    version = getAppVersion(),
                                    cancelled = flag
                                ) { sent, total ->
                                    val pct = if (total > 0) ((sent * 100) / total).toInt() else 0
                                    if (pct != lastPercentB || sent == total) {
                                        lastPercentB = pct
                                        if (!flag.get() && uploadGeneration.get() == gen) {
                                            pathBState = pathBState.copy(bytesSent = sent, totalBytes = total)
                                        }
                                    }
                                }
                                if (flag.get() || uploadGeneration.get() != gen) {
                                    if (flag.get()) logCancel()
                                    return@async
                                }
                                if (uploadGeneration.get() == gen) {
                                    pathBState = pathBState.copy(
                                        url = resB.url,
                                        directUrl = resB.directUrl,
                                        status = "Verifying",
                                        bytesSent = zip.length(),
                                        totalBytes = zip.length()
                                    )
                                }
                                val vB = verifyUpload(resB.directUrl, localSha, getAppVersion(), flag)
                                if (flag.get() || uploadGeneration.get() != gen) {
                                    if (flag.get()) logCancel()
                                    return@async
                                }
                                if (uploadGeneration.get() == gen) {
                                    if (vB == "Verified ✓") {
                                        pathBState = pathBState.copy(status = "Done ✓ verified", verifyStatus = "✓")
                                    } else {
                                        pathBState = pathBState.copy(status = "Failed: $vB", verifyStatus = "✗")
                                    }
                                }
                            } catch (e: CancellationException) {
                                logCancel()
                                throw e
                            } catch (e: Exception) {
                                if (flag.get()) {
                                    logCancel()
                                    throw CancellationException("Upload cancelled")
                                }
                                if (!flag.get() && uploadGeneration.get() == gen) {
                                    val msg = friendlyUploadError(e)
                                    pathBState = pathBState.copy(status = "Failed: $msg", error = msg)
                                }
                            }
                        }

                        jobA.await()
                        jobB.await()
                    }

                    if (flag.get() || uploadGeneration.get() != gen) return@launch
                    isUploading = false
                    if (pathAState.url != null || pathBState.url != null) {
                        showLinkDialog = true
                    } else {
                        showErrorDialog = true
                    }
                } catch (_: CancellationException) {
                    if (flag.get() || uploadGeneration.get() != gen) return@launch
                    if (isUploading) {
                        isUploading = false
                        Toast.makeText(context, "Upload cancelled", Toast.LENGTH_SHORT).show()
                    }
                } catch (e: Exception) {
                    if (flag.get() || uploadGeneration.get() != gen) return@launch
                    isUploading = false
                    val msg = friendlyUploadError(e)
                    if (pathAState.url == null) pathAState = pathAState.copy(status = "Failed: $msg", error = msg)
                    if (pathBState.url == null) pathBState = pathBState.copy(status = "Failed: $msg", error = msg)
                    showErrorDialog = true
                }
            }
        }

        if (cloudConfirmRun != null) {
            AlertDialog(
                onDismissRequest = { cloudConfirmRun = null },
                title = { Text("Send to cloud?") },
                text = {
                    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                        Text("This uploads a ZIP of logs to a public file host (catbox.moe, or gofile.io as fallback) and to the PanVK project's own storage (deleted after 30 days). Anyone with a link can download it. It may contain your device model, GPU info, Android version, app and package names and file paths. It does not include accounts, contacts or personal files. Share the links only in the PanVK Telegram group. Files on catbox/gofile may not be deletable.")
                        if (uploadEndpoint != PANVK_UPLOAD_ENDPOINT) {
                            Text(
                                text = "Test upload endpoint override active: $uploadEndpoint",
                                color = MaterialTheme.colorScheme.error
                            )
                            TextButton(
                                onClick = { uploadEndpoint = PANVK_UPLOAD_ENDPOINT },
                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                modifier = Modifier.height(28.dp)
                            ) {
                                Text("Clear override", style = MaterialTheme.typography.labelSmall)
                            }
                        }
                    }
                },
                confirmButton = {
                    Button(
                        enabled = !isUploading,
                        onClick = {
                            val targetRun = cloudConfirmRun
                            cloudConfirmRun = null
                            if (targetRun != null && !isUploading) {
                                val gen = uploadGeneration.incrementAndGet()
                                val flag = AtomicBoolean(false)
                                currentCancelFlag.value.set(true)
                                currentCancelFlag.value = flag

                                val endpoint = uploadEndpoint
                                val bZipStatus = if (endpoint.isEmpty()) "Skipped (not configured)" else "Preparing ZIP..."
                                val bZipError = if (endpoint.isEmpty()) "Skipped (not configured)" else null
                                pathAState = UploadPathState(name = "catbox / gofile", status = "Preparing ZIP...")
                                pathBState = UploadPathState(name = "PanVK storage (R2)", status = bZipStatus, error = bZipError)
                                isUploading = true
                                showLinkDialog = false
                                showErrorDialog = false

                                coroutineScope.launch {
                                    try {
                                        val zip = buildRunZip(targetRun.folder)
                                        if (flag.get() || uploadGeneration.get() != gen) return@launch
                                        val localSha = withContext(Dispatchers.IO) { sha256(zip) }
                                        if (flag.get() || uploadGeneration.get() != gen) return@launch
                                        startDualUpload(zip, localSha, gen, flag)
                                    } catch (e: Exception) {
                                        if (flag.get() || uploadGeneration.get() != gen) return@launch
                                        isUploading = false
                                        Toast.makeText(context, "Zip failed: ${friendlyUploadError(e)}", Toast.LENGTH_SHORT).show()
                                    }
                                }
                            }
                        }
                    ) {
                        Text("Upload")
                    }
                },
                dismissButton = {
                    OutlinedButton(onClick = { cloudConfirmRun = null }) {
                        Text("Cancel")
                    }
                }
            )
        }

        if (isUploading) {
            AlertDialog(
                onDismissRequest = { /* non-dismissable */ },
                properties = DialogProperties(dismissOnBackPress = false, dismissOnClickOutside = false),
                title = { Text("Uploading...") },
                text = {
                    Column(
                        modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        for (path in listOf(pathAState, pathBState)) {
                            Column(
                                modifier = Modifier.fillMaxWidth(),
                                verticalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Text(path.name, fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.labelMedium)
                                val progress = if (path.totalBytes > 0) {
                                    (path.bytesSent.toFloat() / path.totalBytes.toFloat()).coerceIn(0f, 1f)
                                } else if (path.status.startsWith("Done") || path.status == "Verifying") {
                                    1f
                                } else {
                                    0f
                                }
                                LinearProgressIndicator(
                                    progress = { progress },
                                    modifier = Modifier.fillMaxWidth()
                                )
                                val progressText = if (path.status == "Uploading") {
                                    val percent = (progress * 100).toInt()
                                    val sentMb = path.bytesSent / (1024.0 * 1024.0)
                                    val totalMb = path.totalBytes / (1024.0 * 1024.0)
                                    String.format(Locale.US, "Uploading %.2f / %.2f MB (%d%%)", sentMb, totalMb, percent)
                                } else {
                                    path.status
                                }
                                Text(
                                    text = progressText,
                                    style = MaterialTheme.typography.bodySmall,
                                    modifier = Modifier.align(Alignment.CenterHorizontally)
                                )
                            }
                        }
                    }
                },
                confirmButton = {
                    OutlinedButton(
                        onClick = {
                            currentCancelFlag.value.set(true)
                            uploadGeneration.incrementAndGet()
                            isUploading = false
                            Toast.makeText(context, "Upload cancelled", Toast.LENGTH_SHORT).show()
                        }
                    ) {
                        Text("Cancel")
                    }
                }
            )
        }

        if (showErrorDialog) {
            val errA = pathAState.error ?: pathAState.status
            val errB = pathBState.error ?: pathBState.status
            AlertDialog(
                onDismissRequest = {
                    showErrorDialog = false
                    uploadGeneration.incrementAndGet()
                },
                title = { Text("Upload Failed") },
                text = {
                    SelectionContainer {
                        Text("${pathAState.name}: $errA\n\n${pathBState.name}: $errB")
                    }
                },
                confirmButton = {
                    Button(onClick = {
                        showErrorDialog = false
                        val zip = uploadZipFile
                        val sha = uploadSha256
                        if (zip != null && sha != null) {
                            startDualUpload(zip, sha)
                        }
                    }) {
                        Text("Retry")
                    }
                },
                dismissButton = {
                    OutlinedButton(onClick = {
                        showErrorDialog = false
                        uploadGeneration.incrementAndGet()
                    }) {
                        Text("Cancel")
                    }
                }
            )
        }

        if (showLinkDialog) {
            val hex = uploadSha256 ?: ""
            val shareText = buildString {
                append("PanProbe logs:\n")
                if (pathAState.url != null) append(pathAState.url).append("\n")
                if (pathBState.url != null) append(pathBState.url).append("\n")
                append("SHA-256: $hex")
            }

            AlertDialog(
                onDismissRequest = {
                    showLinkDialog = false
                    uploadGeneration.incrementAndGet()
                },
                title = { Text("Upload done") },
                text = {
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        verticalArrangement = Arrangement.spacedBy(12.dp)
                    ) {
                        for (p in listOf(pathAState, pathBState)) {
                            val isPathA = p.name == pathAState.name
                            Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                                Text(p.name, fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.labelLarge)
                                if (p.url != null) {
                                    SelectionContainer {
                                        Text(
                                            text = p.url,
                                            style = MaterialTheme.typography.bodyMedium,
                                            color = MaterialTheme.colorScheme.primary
                                        )
                                    }
                                    Row(
                                        verticalAlignment = Alignment.CenterVertically,
                                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                                    ) {
                                        val (statusText, statusTone) = when (p.verifyStatus) {
                                            "✓" -> Pair("✓", Tone.Ok)
                                            "– not verified" -> Pair("– not verified", Tone.Neutral)
                                            "✗" -> Pair("✗", Tone.Error)
                                            else -> Pair(p.verifyStatus ?: "", Tone.Neutral)
                                        }
                                        StatusPill(text = statusText, tone = statusTone)

                                        if (p.verifyStatus == "✗" && p.directUrl != null) {
                                            val isRetrying = if (isPathA) isRetryingA else isRetryingB
                                            OutlinedButton(
                                                enabled = !isRetrying,
                                                onClick = {
                                                    val retryGen = uploadGeneration.get()
                                                    coroutineScope.launch {
                                                        if (isPathA) isRetryingA = true else isRetryingB = true
                                                        val currentP = if (isPathA) pathAState else pathBState
                                                        if (isPathA) {
                                                            pathAState = pathAState.copy(status = "Verifying", verifyStatus = "Verifying...")
                                                        } else {
                                                            pathBState = pathBState.copy(status = "Verifying", verifyStatus = "Verifying...")
                                                        }
                                                        val newStatus = withContext(Dispatchers.IO) {
                                                            verifyUpload(currentP.directUrl, hex, getAppVersion(), currentCancelFlag.value)
                                                        }
                                                        if (uploadGeneration.get() != retryGen) return@launch
                                                        val isOk = newStatus == "Verified ✓"
                                                        val updated = currentP.copy(
                                                            status = if (isOk) "Done ✓ verified" else "Failed: $newStatus",
                                                            verifyStatus = if (isOk) "✓" else "✗"
                                                        )
                                                        if (isPathA) pathAState = updated else pathBState = updated
                                                        if (isPathA) isRetryingA = false else isRetryingB = false
                                                    }
                                                },
                                                contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                                modifier = Modifier.height(28.dp)
                                            ) {
                                                Text(if (isRetrying) "Retrying..." else "Retry", style = MaterialTheme.typography.labelSmall)
                                            }
                                        }
                                    }
                                } else {
                                    SelectionContainer {
                                        Text(
                                            text = p.error ?: p.status,
                                            style = MaterialTheme.typography.bodySmall,
                                            color = MaterialTheme.colorScheme.error
                                        )
                                    }
                                }
                            }
                        }

                        SelectionContainer {
                            Text(
                                text = "SHA-256: $hex",
                                style = MaterialTheme.typography.bodySmall,
                                fontFamily = FontFamily.Monospace
                            )
                        }
                    }
                },
                confirmButton = {
                    Column(
                        modifier = Modifier.fillMaxWidth(),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                        ) {
                            Button(
                                onClick = {
                                    val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                                    val clip = ClipData.newPlainText("PanProbe Upload Link", shareText)
                                    clipboard.setPrimaryClip(clip)
                                    Toast.makeText(context, "Link copied to clipboard", Toast.LENGTH_SHORT).show()
                                },
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Copy")
                            }
                            Button(
                                onClick = {
                                    val sendIntent = Intent(Intent.ACTION_SEND).apply {
                                        type = "text/plain"
                                        putExtra(Intent.EXTRA_TEXT, shareText)
                                    }
                                    context.startActivity(Intent.createChooser(sendIntent, "Share Link"))
                                },
                                modifier = Modifier.weight(1f)
                            ) {
                                Text("Share")
                            }
                        }
                        Button(
                            onClick = {
                                val tgIntent = Intent(Intent.ACTION_VIEW, Uri.parse("https://t.me/+E-NhUATmkqE5ODg1"))
                                context.startActivity(tgIntent)
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text("Open Telegram group")
                        }
                        OutlinedButton(
                            onClick = {
                                showLinkDialog = false
                                uploadGeneration.incrementAndGet()
                            },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text("Close")
                        }
                    }
                }
            )
        }

        var showClearConfirmDialog by remember { mutableStateOf(false) }

        if (showClearConfirmDialog) {
            AlertDialog(
                onDismissRequest = { showClearConfirmDialog = false },
                title = { Text("Clear all logs and runs?") },
                text = { Text("This deletes all saved test runs and log files on this device.") },
                confirmButton = {
                    Button(
                        onClick = {
                            showClearConfirmDialog = false
                            val logsDir = File(filesDir, "logs")
                            logsDir.listFiles()?.forEach { it.delete() }
                            val runsDir = File(filesDir, "runs")
                            runsDir.deleteRecursively()
                            selectedLogFileState.value = null
                            selectedLogTextState.value = null
                            refreshLogsList()
                            refreshRunsList()
                        }
                    ) {
                        Text("Clear")
                    }
                },
                dismissButton = {
                    OutlinedButton(onClick = { showClearConfirmDialog = false }) {
                        Text("Cancel")
                    }
                }
            )
        }

        Column(modifier = Modifier.fillMaxSize()) {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.SpaceBetween,
                verticalAlignment = Alignment.CenterVertically
            ) {
                SectionTitle("Logs")
                Button(
                    onClick = {
                        showClearConfirmDialog = true
                    }
                ) {
                    Text("Clear")
                }
            }

            Spacer(Modifier.height(8.dp))

            if (selectedLogFileState.value != null) {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            selectedLogFileState.value?.name ?: "",
                            fontWeight = FontWeight.Bold,
                            style = MaterialTheme.typography.bodyMedium,
                            modifier = Modifier.weight(1f)
                        )
                        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                            Button(
                                onClick = {
                                    val textToShare = (selectedLogTextState.value ?: "").take(400_000)
                                    val sendIntent = Intent(Intent.ACTION_SEND).apply {
                                        type = "text/plain"
                                        putExtra(Intent.EXTRA_TEXT, textToShare)
                                    }
                                    context.startActivity(Intent.createChooser(sendIntent, "Share Log"))
                                }
                            ) {
                                Text("Share")
                            }
                            OutlinedButton(onClick = {
                                selectedLogFileState.value = null
                                selectedLogTextState.value = null
                            }) {
                                Text("Close")
                            }
                        }
                    }
                }

                Spacer(Modifier.height(8.dp))

                Surface(
                    modifier = Modifier.fillMaxSize(),
                    shape = MaterialTheme.shapes.small,
                    color = MaterialTheme.colorScheme.surfaceContainerLowest
                ) {
                    SelectionContainer(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(10.dp)
                            .verticalScroll(rememberScrollState())
                    ) {
                        Text(
                            text = selectedLogTextState.value ?: "",
                            fontFamily = FontFamily.Monospace,
                            fontSize = 11.sp,
                            lineHeight = 14.sp
                        )
                    }
                }
            } else {
                if (runsListState.value.isEmpty() && logFilesListState.value.isEmpty()) {
                    EmptyState(
                        painter = painterResource(R.drawable.ic_tab_logs),
                        title = "No logs yet.",
                        body = "Send the ZIP or link to the PanVK Telegram group so we can check your results."
                    )
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                        contentPadding = PaddingValues(bottom = 16.dp)
                    ) {
                        if (runsListState.value.isNotEmpty()) {
                            item(key = "runs_header") {
                                Column(modifier = Modifier.fillMaxWidth().padding(bottom = 4.dp)) {
                                    SectionTitle("Runs")
                                    Spacer(Modifier.height(2.dp))
                                    Text(
                                        "Send the ZIP or link to the PanVK Telegram group so we can check your results.",
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.onSurfaceVariant
                                    )
                                }
                            }
                            items(runsListState.value, key = { "run_${it.name}" }) { run ->
                                Card(
                                    modifier = Modifier.fillMaxWidth(),
                                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer)
                                ) {
                                    Column(modifier = Modifier.padding(14.dp)) {
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.SpaceBetween,
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            Text(run.name, fontWeight = FontWeight.Bold, style = MaterialTheme.typography.titleMedium)
                                            val runPassTone = if (run.passCount == run.totalCount && run.totalCount > 0) Tone.Ok else Tone.Neutral
                                            StatusPill(
                                                text = "${run.passCount}/${run.totalCount}",
                                                tone = runPassTone
                                            )
                                        }
                                        Spacer(Modifier.height(10.dp))
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                                        ) {
                                            Button(
                                                onClick = { cloudConfirmRun = run },
                                                enabled = !isUploading,
                                                contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp)
                                            ) {
                                                Text("Send to cloud", maxLines = 1)
                                            }
                                            OutlinedButton(
                                                onClick = {
                                                    coroutineScope.launch {
                                                        try {
                                                            val zipFile = buildRunZip(run.folder)
                                                            shareFile(context, zipFile, "application/zip", "Share Run ZIP")
                                                        } catch (e: Exception) {
                                                            Toast.makeText(context, "Zip failed: ${e.message}", Toast.LENGTH_SHORT).show()
                                                        }
                                                    }
                                                },
                                                enabled = !isUploading,
                                                contentPadding = PaddingValues(horizontal = 14.dp, vertical = 6.dp)
                                            ) {
                                                Text("Zip & Share", maxLines = 1)
                                            }
                                        }
                                    }
                                }
                            }
                            if (logFilesListState.value.isNotEmpty()) {
                                item(key = "logs_header") {
                                    Spacer(Modifier.height(4.dp))
                                    SectionTitle("Log Files")
                                }
                            }
                        } else {
                            item(key = "no_runs_hint") {
                                Text(
                                    "Send the ZIP or link to the PanVK Telegram group so we can check your results.",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                        }

                        items(logFilesListState.value, key = { "log_${it.name}" }) { file ->
                            OutlinedCard(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clickable {
                                        selectedLogFileState.value = file
                                        selectedLogTextState.value = if (file.exists()) file.readText() else ""
                                    },
                                colors = CardDefaults.outlinedCardColors(containerColor = MaterialTheme.colorScheme.surface)
                            ) {
                                Row(
                                    modifier = Modifier.padding(14.dp).fillMaxWidth(),
                                    horizontalArrangement = Arrangement.SpaceBetween,
                                    verticalAlignment = Alignment.CenterVertically
                                ) {
                                    Column(modifier = Modifier.weight(1f)) {
                                        Text(file.name, fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.bodyMedium)
                                        Text("${file.length()} bytes", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
