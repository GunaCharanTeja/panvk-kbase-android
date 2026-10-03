package dev.zenithblue.panvktest

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.util.Log
import android.view.SurfaceHolder
import android.view.SurfaceView
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
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
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
        for (test in selected) {
            withContext(Dispatchers.Main) {
                updateTestStatus(test.name, "RUNNING")
            }
            val res = executeTest(test)
            withContext(Dispatchers.Main) {
                updateTestResult(res)
            }
            say("RESULT ${test.name} ${res.status} mismatch=${res.mismatch} fps=${res.fps ?: "0"} ms=${res.durationMs}${if (res.extra != null) " extra=${res.extra}" else ""}")
            if (res.status == "PASS") {
                passCount++
            }
        }
        say("AUTORUN DONE pass=$passCount total=${selected.size}")
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
                    onClick = {
                        coroutineScope.launch {
                            isRunningAllState.value = true
                            for (test in testCases) {
                                updateTestStatus(test.name, "RUNNING")
                                val res = executeTest(test)
                                updateTestResult(res)
                            }
                            isRunningAllState.value = false
                        }
                    }
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
                        selectedLogFileState.value = null
                        selectedLogTextState.value = null
                        refreshLogsList()
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
                if (logFilesListState.value.isEmpty()) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        Text("No logs yet.")
                    }
                } else {
                    LazyColumn(
                        modifier = Modifier.fillMaxSize(),
                        verticalArrangement = Arrangement.spacedBy(4.dp)
                    ) {
                        items(logFilesListState.value) { file ->
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
