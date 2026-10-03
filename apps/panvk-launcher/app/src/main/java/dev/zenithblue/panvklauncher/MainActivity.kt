package dev.zenithblue.panvklauncher

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.util.Log
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.ContextCompat
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val autoprobe = intent?.getBooleanExtra("autoprobe", false) ?: false
        // adb: am start -n dev.zenithblue.panvklauncher/.MainActivity --es run_exe /path/to/game.exe
        val autoRunExe = intent?.getStringExtra("run_exe")
        ShortcutRequests.fromIntent(this, intent) // --es dev.zenithblue.panvklauncher.LAUNCH_SHORTCUT <id|name>
        setContent {
            var themeMode by remember { mutableStateOf(UiPrefs.theme(this@MainActivity)) }
            var dynamicColor by remember { mutableStateOf(UiPrefs.dynamic(this@MainActivity)) }
            PanvkTheme(mode = themeMode, dynamic = dynamicColor) {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    LauncherApp(
                        autoprobe = autoprobe,
                        autoRunExe = autoRunExe,
                        themeMode = themeMode,
                        onThemeMode = { themeMode = it; UiPrefs.setTheme(this@MainActivity, it) },
                        dynamicColor = dynamicColor,
                        onDynamicColor = { dynamicColor = it; UiPrefs.setDynamic(this@MainActivity, it) }
                    )
                }
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        ShortcutRequests.fromIntent(this, intent)
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LauncherApp(
    autoprobe: Boolean,
    autoRunExe: String? = null,
    themeMode: String = "dark",
    onThemeMode: (String) -> Unit = {},
    dynamicColor: Boolean = false,
    onDynamicColor: (Boolean) -> Unit = {}
) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var selectedTab by remember { mutableStateOf(AppTab.Games) }
    var showLogs by remember { mutableStateOf(false) }
    var drivers by remember { mutableStateOf(DriverManager.getDrivers(context)) }
    var selectedDriverId by remember { mutableStateOf(DriverManager.getSelectedDriverId(context)) }

    val selectedDriver = remember(drivers, selectedDriverId) {
        DriverManager.getSelectedDriver(context, drivers)
    }

    var probeResult by remember { mutableStateOf<String?>(null) }
    var isProbing by remember { mutableStateOf(false) }
    var importing by remember { mutableStateOf(false) }
    var installedComponents by remember { mutableStateOf(ContentManager.list(context)) }
    var isComponentBusy by remember { mutableStateOf(false) }
    var componentProgressText by remember { mutableStateOf<String?>(null) }
    var lastComputedSha256 by remember { mutableStateOf<String?>(null) }
    var busyEntryUrl by remember { mutableStateOf<String?>(null) }
    var componentError by remember { mutableStateOf<String?>(null) }
    val logs = remember { mutableStateListOf<String>() }
    val mainHandler = remember { Handler(Looper.getMainLooper()) }

    var isWineRunning by remember { mutableStateOf(ContainerManager.isRunning()) }
    var runningName by remember { mutableStateOf<String?>(null) }
    var isSettingUp by remember { mutableStateOf(false) }
    var isContainerSetup by remember { mutableStateOf(ContainerManager.isSetup(context)) }
    var recentExes by remember { mutableStateOf(ContainerManager.recentExes(context)) }

    fun addLog(msg: String) {
        val time = SimpleDateFormat("HH:mm:ss", Locale.US).format(Date())
        logs.add("[$time] $msg")
    }

    val hasProton = remember(installedComponents) {
        installedComponents.any { it.type == "Proton" }
    }
    val hasImagefs = remember(installedComponents) {
        installedComponents.any { it.type == "imagefs" && it.versionName == "bionic" }
    }

    LaunchedEffect(Unit) {
        while (true) {
            isWineRunning = ContainerManager.isRunning()
            delay(500)
        }
    }

    LaunchedEffect(hasProton, hasImagefs) {
        if (hasProton && hasImagefs && !ContainerManager.isSetup(context) && !isSettingUp) {
            isSettingUp = true
            try {
                addLog("Setting up container...")
                val result = withContext(Dispatchers.IO) {
                    ContainerManager.setup(context) { line ->
                        mainHandler.post { addLog(line) }
                    }
                }
                withContext(Dispatchers.Main) {
                    if (result.isSuccess) {
                        addLog("Container setup complete.")
                    } else {
                        addLog("Container setup failed: ${result.exceptionOrNull()?.message}")
                    }
                }
            } catch (t: Throwable) {
                if (t !is kotlinx.coroutines.CancellationException) {
                    addLog("Container setup failed: ${t.message}")
                }
            } finally {
                isSettingUp = false
                isContainerSetup = ContainerManager.isSetup(context)
            }
        }
    }

    fun runWineArgs(args: List<String>) {
        if (isWineRunning || isSettingUp) return
        isWineRunning = true
        runningName = "wine " + args.joinToString(" ")
        scope.launch(Dispatchers.IO) {
            try {
                ContainerManager.run(context, args) { line ->
                    mainHandler.post { addLog(line) }
                }
            } catch (t: Throwable) {
                mainHandler.post { addLog("Run error: ${t.message}") }
            } finally {
                mainHandler.post {
                    isWineRunning = ContainerManager.isRunning()
                    isContainerSetup = ContainerManager.isSetup(context)
                }
            }
        }
    }

    fun runWineExe(exePath: String, sc: Shortcut? = null) {
        if (isWineRunning || isSettingUp) return
        isWineRunning = true
        runningName = sc?.name ?: File(exePath).nameWithoutExtension
        scope.launch(Dispatchers.IO) {
            val startMs = System.currentTimeMillis()
            val launcherLog = StringBuffer()
            var launched = false
            try {
                // Controller mapping (shortcut file, exe preset or default) must be set before Wine/X server start.
                ControllerInput.config = ControllerConfig.resolve(context, sc?.id, exePath)
                ContainerManager.addRecent(context, exePath)
                mainHandler.post {
                    recentExes = ContainerManager.recentExes(context)
                }
                if (sc != null) {
                    // Shortcut: per-game resolution (global display pref), lastPlayed, args/env/driver via LaunchOptions.
                    // Game's own size applies to this run only; the saved default stays as it is.
                    DisplayServer.launchOverride = sc.resolution.takeIf { Resolution.parse(it) != null }
                    ShortcutStore.save(context, sc.copy(lastPlayed = System.currentTimeMillis()))
                    mainHandler.post { addLog("Launch shortcut '${sc.name}' (${sc.id})") }
                }
                launched = true
                ContainerManager.runExe(context, exePath, { line ->
                    launcherLog.append(line).append('\n')
                    mainHandler.post { addLog(line) }
                }, sc?.let { ShortcutStore.launchOptions(context, it) })
            } catch (t: Throwable) {
                launcherLog.append("Run error: ${t.message}\n")
                mainHandler.post { addLog("Run error: ${t.message}") }
            } finally {
                DisplayServer.launchOverride = null
                mainHandler.post {
                    isWineRunning = ContainerManager.isRunning()
                    isContainerSetup = ContainerManager.isSetup(context)
                }
                // Game returned (exit, crash or driver failure): collect every log and show the Session logs screen.
                try {
                    val dir = SessionLogs.collect(context, sc, exePath, startMs, launcherLog.toString(), launched)
                    mainHandler.post { SessionLogsActivity.open(context, dir.name) }
                } catch (t: Throwable) {
                    mainHandler.post { addLog("Session logs failed: $t") }
                }
            }
        }
    }

    fun runExplorer() {
        if (isWineRunning || isSettingUp) return
        isWineRunning = true
        runningName = "Wine Explorer"
        scope.launch(Dispatchers.IO) {
            try {
                ContainerManager.runExplorer(context) { line ->
                    mainHandler.post { addLog(line) }
                }
            } catch (t: Throwable) {
                mainHandler.post { addLog("Explorer run error: ${t.message}") }
            } finally {
                mainHandler.post {
                    isWineRunning = ContainerManager.isRunning()
                    isContainerSetup = ContainerManager.isSetup(context)
                }
            }
        }
    }

    fun stopWine() {
        scope.launch(Dispatchers.IO) {
            try {
                mainHandler.post { addLog("Stopping Wine...") }
                ContainerManager.stop(context)
                mainHandler.post { addLog("Wine stopped.") }
            } catch (t: Throwable) {
                mainHandler.post { addLog("Stop error: ${t.message}") }
            } finally {
                mainHandler.post {
                    isWineRunning = ContainerManager.isRunning()
                }
            }
        }
    }

    var isDxvkEnabled by remember { mutableStateOf(ContainerManager.isDxvkEnabled(context)) }

    fun toggleDxvk(enabled: Boolean) {
        scope.launch(Dispatchers.IO) {
            val err = ContainerManager.setDxvkEnabled(context, enabled)
            withContext(Dispatchers.Main) {
                isDxvkEnabled = ContainerManager.isDxvkEnabled(context)
                if (err != null) {
                    addLog("DXVK error: $err")
                } else {
                    addLog("DXVK ${if (enabled) "enabled" else "disabled"}")
                }
            }
        }
    }

    var displayRev by remember { mutableStateOf(0) }
    val builtinDisplay = displayRev.let { DisplayServer.mode(context) == DisplayServer.Mode.BUILTIN }
    val displayRes = displayRev.let { DisplayServer.resolution(context) }
    val displayShm = displayRev.let { DisplayServer.useShm(context) }

    fun openScreen() {
        val intent = Intent(context, ScreenActivity::class.java)
        context.startActivity(intent)
    }

    val exePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            scope.launch(Dispatchers.IO) {
                mainHandler.post { addLog("Importing URI: $uri...") }
                val resolvedPath = ContainerManager.importUri(context, uri)
                if (resolvedPath != null) {
                    mainHandler.post { addLog("Imported: $resolvedPath") }
                    withContext(Dispatchers.Main) {
                        runWineExe(resolvedPath)
                    }
                } else {
                    mainHandler.post { addLog("Failed to import URI: $uri") }
                }
            }
        }
    }

    var pendingStoragePath by rememberSaveable { mutableStateOf<String?>(null) }
    val storagePermissionLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.RequestMultiplePermissions()
    ) {
        val path = pendingStoragePath
        pendingStoragePath = null
        if (path != null) {
            if (path == "__PICK__") {
                exePickerLauncher.launch(arrayOf("*/*"))
            } else if (path.startsWith("__SC__:")) {
                ShortcutStore.find(context, path.removePrefix("__SC__:"))?.let {
                    runWineExe(ShortcutStore.resolveExe(context, it.exe), it)
                }
            } else {
                runWineExe(path)
            }
        }
    }

    fun hasStoragePermission(): Boolean {
        val readGranted = ContextCompat.checkSelfPermission(
            context,
            android.Manifest.permission.READ_EXTERNAL_STORAGE
        ) == PackageManager.PERMISSION_GRANTED
        val writeGranted = ContextCompat.checkSelfPermission(
            context,
            android.Manifest.permission.WRITE_EXTERNAL_STORAGE
        ) == PackageManager.PERMISSION_GRANTED
        return readGranted && writeGranted
    }

    fun launchExePickerWithPermission() {
        if (hasStoragePermission()) {
            exePickerLauncher.launch(arrayOf("*/*"))
        } else {
            pendingStoragePath = "__PICK__"
            storagePermissionLauncher.launch(
                arrayOf(
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                )
            )
        }
    }

    fun runExeWithPermission(path: String, sc: Shortcut? = null) {
        val appFiles = context.filesDir.absolutePath
        val appData = context.applicationInfo.dataDir
        val privatePath = path.startsWith("$appFiles/") || path.startsWith("$appData/")
        if (privatePath || hasStoragePermission()) {
            runWineExe(path, sc)
        } else {
            pendingStoragePath = if (sc != null) "__SC__:${sc.id}" else path
            storagePermissionLauncher.launch(
                arrayOf(
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE
                )
            )
        }
    }

    fun runProbe(driver: Driver) {
        if (isProbing) return
        isProbing = true
        scope.launch {
            try {
                addLog("Probing driver: ${driver.name} (${driver.libPath})")
                val result = try {
                    withContext(Dispatchers.IO) {
                        Native.probe(driver.libPath)
                    }
                } catch (t: Throwable) {
                    "FAIL exception: ${t}"
                }
                probeResult = result
                addLog("Probe (${driver.name}):\n$result")
                Log.i("PanVKLauncher", result)
            } finally {
                isProbing = false
            }
        }
    }

    // Auto-probe when launched with intent extra autoprobe=true
    var autoprobeTriggered by remember { mutableStateOf(false) }
    LaunchedEffect(autoprobe) {
        if (autoprobe && !autoprobeTriggered) {
            autoprobeTriggered = true
            runProbe(selectedDriver)
        }
    }

    var autoRunTriggered by remember { mutableStateOf(false) }
    LaunchedEffect(autoRunExe, isContainerSetup) {
        if (autoRunExe != null && !autoRunTriggered && isContainerSetup && !isWineRunning) {
            autoRunTriggered = true
            runWineExe(autoRunExe)
        }
    }

    fun runShortcut(sc: Shortcut) = runExeWithPermission(ShortcutStore.resolveExe(context, sc.exe), sc)

    // adb / script: LAUNCH_SHORTCUT intent extra (debug builds). Waits for container setup + idle Wine.
    val shortcutReq = ShortcutRequests.pending.value
    LaunchedEffect(shortcutReq, isContainerSetup, isSettingUp) {
        if (shortcutReq != null && isContainerSetup && !isSettingUp) {
            ShortcutRequests.pending.value = null
            val sc = ShortcutStore.find(context, shortcutReq)
            if (sc == null) addLog("Shortcut not found: $shortcutReq")
            else if (ContainerManager.isRunning()) addLog("Wine busy, not launching '${sc.name}' (stop it first)")
            else runShortcut(sc)
        }
    }

    val importLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            importing = true
            scope.launch {
                try {
                    addLog("Importing package from $uri...")
                    val result = DriverManager.importDriver(context, uri)
                    result.onSuccess { imported ->
                        drivers = DriverManager.getDrivers(context)
                        selectedDriverId = imported.id
                        DriverManager.setSelectedDriverId(context, imported.id)
                        addLog("Imported driver: ${imported.name} (${imported.version})")
                    }.onFailure { err ->
                        addLog("Import failed: ${err.message}")
                    }
                } finally {
                    importing = false
                }
            }
        }
    }

    val componentImportLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (isComponentBusy || uri == null) return@rememberLauncherForActivityResult
        isComponentBusy = true
        componentProgressText = "Copying local file..."
        scope.launch {
            try {
                addLog("Copying local .wcp from $uri...")
                val copyResult = ContentManager.copyFromUri(context, uri)
                copyResult.onSuccess { (file, sha256) ->
                    lastComputedSha256 = sha256
                    addLog("Local file copied: ${file.name}")
                    addLog("UNVERIFIED sha256=$sha256")
                    addLog("Installing component...")
                    componentProgressText = "Installing..."
                    try {
                        val installResult = ContentManager.install(context, file)
                        installResult.onSuccess { installed ->
                            val updatedList = withContext(Dispatchers.IO) {
                                ContentManager.list(context)
                            }
                            installedComponents = updatedList
                            addLog("Installed component: ${installed.type}/${installed.versionName}")
                        }.onFailure { err ->
                            addLog("Installation failed: ${err.message}")
                        }
                    } finally {
                        file.delete()
                    }
                }.onFailure { err ->
                    addLog("Copy failed: ${err.message}")
                }
            } finally {
                isComponentBusy = false
                componentProgressText = null
            }
        }
    }

    fun downloadComponent(st: ComponentState) {
        val entry = st.entry
        if (isComponentBusy) return
        isComponentBusy = true
        busyEntryUrl = entry.url
        componentError = null
        componentProgressText = "Downloading 0.0 MB..."
        scope.launch {
            try {
                // Broken install of the same version must be removed first (install refuses an existing dir).
                if (st.status == ComponentStatus.Incomplete) {
                    withContext(Dispatchers.IO) { st.installed.forEach { ContentManager.delete(context, it) } }
                }
                addLog("Downloading ${entry.name}...")
                val dlResult = ContentManager.download(
                    context = context,
                    url = entry.url,
                    expectedSha256 = entry.sha256,
                    onProgress = { bytes ->
                        val mb = bytes.toDouble() / (1024.0 * 1024.0)
                        componentProgressText = "Downloading %.1f MB...".format(Locale.US, mb)
                    }
                )
                dlResult.onSuccess { (file, sha256) ->
                    lastComputedSha256 = sha256
                    if (entry.sha256 != null) {
                        addLog("SHA-256 (verified): $sha256")
                    } else {
                        addLog("UNVERIFIED sha256=$sha256")
                    }
                    addLog("Installing ${entry.name}...")
                    componentProgressText = "Installing..."
                    try {
                        val installResult = ContentManager.install(context, file, rootfs = (entry.type == "imagefs"))
                        installResult.onSuccess { installed ->
                            val updatedList = withContext(Dispatchers.IO) {
                                // Update: new version is in place, drop the superseded one(s).
                                if (st.status == ComponentStatus.UpdateAvailable) {
                                    st.installed.filter { it.dir != installed.dir }.forEach { ContentManager.delete(context, it) }
                                }
                                ContentManager.list(context)
                            }
                            installedComponents = updatedList
                            addLog("Installed component: ${installed.type}/${installed.versionName}")
                        }.onFailure { err ->
                            componentError = "Install failed: ${err.message}"
                            addLog("Installation failed: ${err.message}")
                        }
                    } finally {
                        file.delete()
                    }
                }.onFailure { err ->
                    componentError = "Download failed: ${err.message}"
                    addLog("Download failed: ${err.message}")
                }
            } finally {
                isComponentBusy = false
                busyEntryUrl = null
                componentProgressText = null
            }
        }
    }

    fun deleteComponent(c: InstalledContent) {
        if (isComponentBusy) return
        isComponentBusy = true
        scope.launch {
            try {
                val label = "${c.type}/${c.versionName}"
                val (deleted, updatedList) = withContext(Dispatchers.IO) {
                    val deleted = ContentManager.delete(context, c)
                    val list = ContentManager.list(context)
                    Pair(deleted, list)
                }
                installedComponents = updatedList
                isContainerSetup = ContainerManager.isSetup(context)
                if (deleted) {
                    addLog("Deleted component: $label")
                } else {
                    addLog("Failed to delete component: $label")
                }
            } finally {
                isComponentBusy = false
            }
        }
    }

    val wineBusy = isWineRunning || isSettingUp
    AppShell(
        tab = selectedTab,
        onTab = { selectedTab = it },
        showLogs = showLogs,
        onShowLogs = { showLogs = it },
        running = wineBusy,
        logs = logs,
        onClearLogs = { logs.clear() },
        onOpenSessionLogs = { SessionLogsActivity.open(context) }
    ) { shown ->
        when (shown) {
            null -> LogsScreen(logs)
            AppTab.Games -> GamesScreen(
                drivers = drivers,
                activeDriver = selectedDriver,
                defaultResolution = displayRes,
                busy = wineBusy,
                runningName = if (isSettingUp) "Setting up container" else runningName,
                onStop = { stopWine() },
                onLaunch = { runShortcut(it) }
            )
            AppTab.Controls -> ControlsScreen(onLog = { addLog(it) })
            AppTab.Components -> ComponentsScreen(
                catalog = ContentManager.CATALOG,
                installedList = installedComponents,
                busy = isComponentBusy,
                busyEntryUrl = busyEntryUrl,
                progressText = componentProgressText,
                errorText = componentError,
                onDismissError = { componentError = null },
                onInstallLocalClick = { componentImportLauncher.launch(arrayOf("*/*")) },
                onDownload = { st -> downloadComponent(st) },
                onDelete = { component -> deleteComponent(component) }
            )
            AppTab.Drivers -> DriversScreen(
                drivers = drivers,
                selectedDriver = selectedDriver,
                probeResult = probeResult,
                isProbing = isProbing,
                importing = importing,
                onSelectDriver = { driver ->
                    selectedDriverId = driver.id
                    DriverManager.setSelectedDriverId(context, driver.id)
                },
                onImportClick = { importLauncher.launch(arrayOf("application/zip", "*/*")) },
                onProbeClick = { runProbe(selectedDriver) },
                onDeleteDriver = { driver ->
                    val driverName = driver.name
                    if (DriverManager.deleteDriver(context, driver)) {
                        drivers = DriverManager.getDrivers(context)
                        if (selectedDriverId == driver.id) {
                            selectedDriverId = DriverManager.bundledId(context)
                            DriverManager.setSelectedDriverId(context, DriverManager.bundledId(context))
                        }
                        addLog("Deleted driver: $driverName")
                    }
                }
            )
            AppTab.Settings -> SettingsScreen(
                themeMode = themeMode,
                onThemeMode = onThemeMode,
                dynamicColor = dynamicColor,
                onDynamicColor = onDynamicColor,
                isSetup = isContainerSetup,
                isSettingUp = isSettingUp,
                isRunning = isWineRunning,
                installedComponents = installedComponents,
                recentExes = recentExes,
                selectedDriver = selectedDriver,
                isDxvkEnabled = isDxvkEnabled,
                onToggleDxvk = { toggleDxvk(it) },
                displayStatus = displayRev.let { DisplayServer.describe(context) },
                builtinDisplay = builtinDisplay,
                onToggleBuiltin = {
                    DisplayServer.setMode(context, if (it) DisplayServer.Mode.BUILTIN else DisplayServer.Mode.TERMUX)
                    displayRev++
                },
                displayRes = displayRes,
                onSetRes = { DisplayServer.setResolution(context, it); displayRev++ },
                displayShm = displayShm,
                onToggleShm = { DisplayServer.setShm(context, it); displayRev++ },
                onOpenDisplay = {
                    val err = DisplayServer.open(context)
                    if (err != null) addLog(err) else addLog("Opened display")
                },
                onOpenScreen = { openScreen() },
                onLaunchExplorer = { runExplorer() },
                onRunCmdVer = { runWineArgs(listOf("cmd", "/c", "ver")) },
                onPickExe = { launchExePickerWithPermission() },
                onRunManualExe = { path -> runExeWithPermission(path) },
                onRunRecentExe = { path -> runExeWithPermission(path) },
                onStop = { stopWine() },
                onOpenAppLog = { showLogs = true },
                onOpenSessionLogs = { SessionLogsActivity.open(context) },
                onOpenControls = { selectedTab = AppTab.Controls }
            )
        }
    }
}
