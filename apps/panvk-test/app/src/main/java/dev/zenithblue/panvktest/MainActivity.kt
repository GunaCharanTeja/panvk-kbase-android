package dev.zenithblue.panvktest

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
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
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.DialogProperties
import androidx.lifecycle.lifecycleScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

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

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Read intent extras for headless testing
        val driverExtra = intent.getStringExtra("driver")
        if (driverExtra != null) {
            when (driverExtra.lowercase(Locale.US)) {
                "bundled" -> driverTypeState.value = DriverType.BUNDLED
                "system" -> driverTypeState.value = DriverType.SYSTEM
                "imported" -> driverTypeState.value = DriverType.IMPORTED
            }
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
            MaterialTheme {
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
        val logsDir = File(filesDir, "logs")
        if (logsDir.exists()) {
            logFilesListState.value = (logsDir.listFiles() ?: emptyArray())
                .filter { it.isFile }
                .sortedByDescending { it.lastModified() }
        } else {
            logFilesListState.value = emptyList()
        }
    }

    private fun refreshRunsList() {
        val runsDir = File(filesDir, "runs")
        if (runsDir.exists()) {
            val folders = (runsDir.listFiles() ?: emptyArray())
                .filter { it.isDirectory }
                .sortedByDescending { it.name }
            runsListState.value = folders.map { folder ->
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
            runsListState.value = emptyList()
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
        val zipFile = File(shareDir, "panprobe-${runFolder.name}.zip")
        val entries = mutableListOf<Pair<String, File>>()
        entries.add(Pair(runFolder.name, runFolder))

        if (infoRawJsonState.value != null) {
            val vkFile = File(shareDir, "vulkan-info.json").apply { writeText(infoRawJsonState.value!!) }
            entries.add(Pair("vulkan-info.json", vkFile))
        }

        val logcatFile = File(shareDir, "logcat.txt")
        try {
            val process = Runtime.getRuntime().exec(
                arrayOf("logcat", "-d", "-v", "threadtime", "--pid=" + android.os.Process.myPid())
            )
            val logcatText = process.inputStream.bufferedReader().use { it.readText() }
            logcatFile.writeText(logcatText)
            entries.add(Pair("logcat.txt", logcatFile))
        } catch (_: Exception) {}

        zipFiles(zipFile, entries)
        zipFile
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

    @Suppress("DEPRECATION")
    @Composable
    fun MainScreen() {
        Column(modifier = Modifier.fillMaxSize()) {
            val tabs = listOf("Driver", "Info", "Tests", "Logs")
            TabRow(selectedTabIndex = selectedTabState.intValue) {
                tabs.forEachIndexed { index, title ->
                    Tab(
                        selected = selectedTabState.intValue == index,
                        onClick = { selectedTabState.intValue = index },
                        text = { Text(title) }
                    )
                }
            }

            Box(modifier = Modifier.fillMaxSize().padding(16.dp)) {
                when (selectedTabState.intValue) {
                    0 -> DriverTab()
                    1 -> InfoTab()
                    2 -> TestsTab()
                    3 -> LogsTab()
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
                } catch (e: Exception) {
                    importedFileNameState.value = "Error: ${e.message}"
                }
            }
        }

        Column(
            modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState()),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            Text("Vulkan Driver Selection", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)

            DriverType.entries.forEach { type ->
                Row(
                    modifier = Modifier.fillMaxWidth().clickable { driverTypeState.value = type },
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    RadioButton(
                        selected = driverTypeState.value == type,
                        onClick = { driverTypeState.value = type }
                    )
                    Spacer(Modifier.width(8.dp))
                    Column {
                        Text(type.label, fontWeight = FontWeight.SemiBold)
                        Text(
                            text = getDriverPath(type),
                            style = MaterialTheme.typography.bodySmall,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                }
            }

            if (driverTypeState.value == DriverType.IMPORTED) {
                Button(onClick = { pickLauncher.launch(arrayOf("*/*")) }) {
                    Text("Select .so file")
                }
                importedFileNameState.value?.let {
                    Text("Imported file: $it", style = MaterialTheme.typography.bodyMedium)
                }
            }

            HorizontalDivider()

            Text("Mesa Environment", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)

            Row(verticalAlignment = Alignment.CenterVertically) {
                Checkbox(
                    checked = mesaDebugEnabledState.value,
                    onCheckedChange = { mesaDebugEnabledState.value = it }
                )
                Spacer(Modifier.width(8.dp))
                Text("Mesa debug env")
            }

            OutlinedTextField(
                value = mesaDebugStrState.value,
                onValueChange = { mesaDebugStrState.value = it },
                label = { Text("Debug Variables (space-separated)") },
                modifier = Modifier.fillMaxWidth()
            )

            Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)) {
                Column(Modifier.padding(12.dp)) {
                    Text("Always passed:", fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.bodySmall)
                    Text("• MESA_LOG=file", style = MaterialTheme.typography.bodySmall)
                    Text("• TMPDIR=${context.cacheDir.absolutePath}", style = MaterialTheme.typography.bodySmall)
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
            hasRuns = runsListState.value.isNotEmpty(),
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
                Text("Tests", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                Button(
                    enabled = !isRunningAllState.value,
                    onClick = { startRunAll() }
                ) {
                    Text(if (isRunningAllState.value) "Running..." else "Run all")
                }
            }

            Spacer(Modifier.height(8.dp))

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
                modifier = Modifier.fillMaxWidth().height(160.dp)
            )

            LazyColumn(
                modifier = Modifier.weight(1f),
                verticalArrangement = Arrangement.spacedBy(8.dp)
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
        val statusColor = when (result.status) {
            "PASS" -> Color(0xFF2E7D32)
            "FAIL" -> Color(0xFFC62828)
            "CRASH" -> Color(0xFF6A1B9A)
            "TIMEOUT" -> Color(0xFFE65100)
            "RUNNING" -> Color(0xFF1565C0)
            else -> MaterialTheme.colorScheme.onSurfaceVariant
        }

        Card(
            modifier = Modifier.fillMaxWidth().clickable { onToggleExpand() },
            colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
        ) {
            Column(modifier = Modifier.padding(12.dp)) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
                    horizontalArrangement = Arrangement.SpaceBetween,
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(test.name, fontWeight = FontWeight.Bold)
                            if (test.isDraw) {
                                Spacer(Modifier.width(6.dp))
                                Text(
                                    "[draw]",
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.primary
                                )
                            }
                        }
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                result.status,
                                fontWeight = FontWeight.Bold,
                                color = statusColor,
                                style = MaterialTheme.typography.bodySmall
                            )
                            if (result.status != "IDLE" && result.status != "RUNNING") {
                                Text("mismatch=${result.mismatch}", style = MaterialTheme.typography.bodySmall)
                                if (result.fps != null) {
                                    Text("FPS ${result.fps}", style = MaterialTheme.typography.bodySmall)
                                }
                                Text("${result.durationMs}ms", style = MaterialTheme.typography.bodySmall)
                            }
                        }
                        result.extra?.let {
                            Text(it, style = MaterialTheme.typography.bodySmall)
                        }
                    }

                    Button(
                        onClick = onRunClick,
                        enabled = result.status != "RUNNING" && !isRunningAllState.value,
                        contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp)
                    ) {
                        Text("Run")
                    }
                }

                if (result.isExpanded && result.lastLines.isNotEmpty()) {
                    Spacer(Modifier.height(8.dp))
                    HorizontalDivider()
                    Spacer(Modifier.height(4.dp))
                    Text("Last ${result.lastLines.size} lines:", style = MaterialTheme.typography.bodySmall, fontWeight = FontWeight.SemiBold)
                    Box(
                        modifier = Modifier
                            .fillMaxWidth()
                            .background(MaterialTheme.colorScheme.surface)
                            .padding(8.dp)
                    ) {
                        SelectionContainer {
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
        var uploadProgress by remember { mutableFloatStateOf(0f) }
        var uploadErrorMsg by remember { mutableStateOf<String?>(null) }
        var uploadSuccessUrl by remember { mutableStateOf<String?>(null) }

        if (cloudConfirmRun != null) {
            AlertDialog(
                onDismissRequest = { cloudConfirmRun = null },
                title = { Text("Send to cloud?") },
                text = {
                    Text("This uploads a ZIP of logs to a public file host (catbox.moe). Anyone with the link can download it. It may contain your device model, GPU info, Android version, app and package names and file paths. It does not include accounts, contacts or personal files. Share the link only in the PanVK Telegram group. Files may not be deletable.")
                },
                confirmButton = {
                    Button(
                        onClick = {
                            val targetRun = cloudConfirmRun
                            cloudConfirmRun = null
                            if (targetRun != null) {
                                coroutineScope.launch {
                                    isUploading = true
                                    uploadProgress = 0f
                                    try {
                                        val zip = buildRunZip(targetRun.folder)
                                        val url = withContext(Dispatchers.IO) {
                                            uploadToCloud(zip, getAppVersion()) { prog ->
                                                uploadProgress = prog
                                            }
                                        }
                                        isUploading = false
                                        uploadSuccessUrl = url
                                    } catch (e: Exception) {
                                        isUploading = false
                                        uploadErrorMsg = e.message ?: "Upload failed"
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
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        @Suppress("DEPRECATION")
                        LinearProgressIndicator(
                            progress = uploadProgress,
                            modifier = Modifier.fillMaxWidth()
                        )
                        Text(
                            text = "${(uploadProgress * 100).toInt()}%",
                            style = MaterialTheme.typography.bodySmall,
                            modifier = Modifier.align(Alignment.End)
                        )
                    }
                },
                confirmButton = {}
            )
        }

        if (uploadErrorMsg != null) {
            AlertDialog(
                onDismissRequest = { uploadErrorMsg = null },
                title = { Text("Upload Failed") },
                text = {
                    SelectionContainer {
                        Text(uploadErrorMsg ?: "")
                    }
                },
                confirmButton = {
                    Button(onClick = { uploadErrorMsg = null }) {
                        Text("OK")
                    }
                }
            )
        }

        if (uploadSuccessUrl != null) {
            val url = uploadSuccessUrl!!
            AlertDialog(
                onDismissRequest = { uploadSuccessUrl = null },
                title = { Text("Upload done") },
                text = {
                    SelectionContainer {
                        Text(
                            text = url,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.primary
                        )
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
                                    val clip = ClipData.newPlainText("PanProbe Upload Link", url)
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
                                        putExtra(Intent.EXTRA_TEXT, url)
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
                            onClick = { uploadSuccessUrl = null },
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            Text("Close")
                        }
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
                Text("Logs", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
                Button(
                    onClick = {
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
            }

            Spacer(Modifier.height(8.dp))

            if (selectedLogFileState.value != null) {
                Row(
                    modifier = Modifier.fillMaxWidth(),
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

                Spacer(Modifier.height(8.dp))

                Box(
                    modifier = Modifier
                        .fillMaxSize()
                        .background(MaterialTheme.colorScheme.surfaceVariant)
                        .padding(8.dp)
                ) {
                    SelectionContainer(
                        modifier = Modifier.fillMaxSize().verticalScroll(rememberScrollState())
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
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Column(
                            horizontalAlignment = Alignment.CenterHorizontally,
                            modifier = Modifier.padding(16.dp)
                        ) {
                            Text("No logs yet.")
                            Spacer(Modifier.height(4.dp))
                            Text(
                                "Send the ZIP or link to the PanVK Telegram group so we can check your results.",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                textAlign = TextAlign.Center
                            )
                        }
                    }
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        verticalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        if (runsListState.value.isNotEmpty()) {
                            item(key = "runs_header") {
                                Column(modifier = Modifier.fillMaxWidth().padding(bottom = 4.dp)) {
                                    Text("Runs", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
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
                                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                                ) {
                                    Column(modifier = Modifier.padding(12.dp)) {
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.SpaceBetween,
                                            verticalAlignment = Alignment.CenterVertically
                                        ) {
                                            Text(run.name, fontWeight = FontWeight.Bold, style = MaterialTheme.typography.bodyLarge)
                                            Text(
                                                "${run.passCount}/${run.totalCount}",
                                                fontWeight = FontWeight.Bold,
                                                style = MaterialTheme.typography.bodyMedium,
                                                color = if (run.passCount == run.totalCount && run.totalCount > 0) Color(0xFF2E7D32) else MaterialTheme.colorScheme.onSurfaceVariant
                                            )
                                        }
                                        Spacer(Modifier.height(8.dp))
                                        Row(
                                            modifier = Modifier.fillMaxWidth(),
                                            horizontalArrangement = Arrangement.spacedBy(8.dp)
                                        ) {
                                            Button(
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
                                                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp)
                                            ) {
                                                Text("Zip & Share", maxLines = 1)
                                            }
                                            OutlinedButton(
                                                onClick = { cloudConfirmRun = run },
                                                contentPadding = PaddingValues(horizontal = 12.dp, vertical = 4.dp)
                                            ) {
                                                Text("Send to cloud", maxLines = 1)
                                            }
                                        }
                                    }
                                }
                            }
                            if (logFilesListState.value.isNotEmpty()) {
                                item(key = "logs_header") {
                                    Spacer(Modifier.height(4.dp))
                                    Text("Log Files", style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.Bold)
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
                            Card(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clickable {
                                        selectedLogFileState.value = file
                                        selectedLogTextState.value = if (file.exists()) file.readText() else ""
                                    },
                                colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant)
                            ) {
                                Row(
                                    modifier = Modifier.padding(12.dp).fillMaxWidth(),
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
