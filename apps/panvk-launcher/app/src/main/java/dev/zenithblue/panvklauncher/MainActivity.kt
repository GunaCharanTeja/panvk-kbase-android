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
            MaterialTheme {
                Surface(
                    modifier = Modifier.fillMaxSize(),
                    color = MaterialTheme.colorScheme.background
                ) {
                    LauncherApp(autoprobe = autoprobe, autoRunExe = autoRunExe)
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

enum class LauncherTab(val title: String, val iconSymbol: String) {
    Drivers("Drivers", "⚙"),
    Components("Components", "🧩"),
    Wine("Wine", "🍷"),
    Games("Games", "🎮"),
    Logs("Logs", "📋")
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LauncherApp(autoprobe: Boolean, autoRunExe: String? = null) {
    val context = LocalContext.current
    val scope = rememberCoroutineScope()

    var selectedTab by remember { mutableStateOf(LauncherTab.Drivers) }
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
    val logs = remember { mutableStateListOf<String>() }
    val mainHandler = remember { Handler(Looper.getMainLooper()) }

    var isWineRunning by remember { mutableStateOf(ContainerManager.isRunning()) }
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
        scope.launch(Dispatchers.IO) {
            try {
                ContainerManager.addRecent(context, exePath)
                mainHandler.post {
                    recentExes = ContainerManager.recentExes(context)
                }
                if (sc != null) {
                    // Shortcut: per-game resolution (global display pref), lastPlayed, args/env/driver via LaunchOptions.
                    if (sc.resolution in BuiltinXServer.RESOLUTIONS) DisplayServer.setResolution(context, sc.resolution)
                    ShortcutStore.save(context, sc.copy(lastPlayed = System.currentTimeMillis()))
                    mainHandler.post { addLog("Launch shortcut '${sc.name}' (${sc.id})") }
                }
                ContainerManager.runExe(context, exePath, { line ->
                    mainHandler.post { addLog(line) }
                }, sc?.let { ShortcutStore.launchOptions(context, it) })
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

    fun runExplorer() {
        if (isWineRunning || isSettingUp) return
        isWineRunning = true
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

    fun downloadComponent(entry: CatalogEntry) {
        if (isComponentBusy) return
        isComponentBusy = true
        componentProgressText = "Downloading 0.0 MB..."
        scope.launch {
            try {
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
                    addLog("Download failed: ${err.message}")
                }
            } finally {
                isComponentBusy = false
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

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("PanVK Launcher") }
            )
        },
        bottomBar = {
            NavigationBar {
                NavigationBarItem(
                    selected = selectedTab == LauncherTab.Drivers,
                    onClick = { selectedTab = LauncherTab.Drivers },
                    label = { Text("Drivers") },
                    icon = { Text(LauncherTab.Drivers.iconSymbol, fontSize = 18.sp) }
                )
                NavigationBarItem(
                    selected = selectedTab == LauncherTab.Components,
                    onClick = { selectedTab = LauncherTab.Components },
                    label = { Text("Components") },
                    icon = { Text(LauncherTab.Components.iconSymbol, fontSize = 18.sp) }
                )
                NavigationBarItem(
                    selected = selectedTab == LauncherTab.Wine,
                    onClick = { selectedTab = LauncherTab.Wine },
                    label = { Text("Wine") },
                    icon = { Text(LauncherTab.Wine.iconSymbol, fontSize = 18.sp) }
                )
                NavigationBarItem(
                    selected = selectedTab == LauncherTab.Games,
                    onClick = { selectedTab = LauncherTab.Games },
                    label = { Text("Games") },
                    icon = { Text(LauncherTab.Games.iconSymbol, fontSize = 18.sp) }
                )
                NavigationBarItem(
                    selected = selectedTab == LauncherTab.Logs,
                    onClick = { selectedTab = LauncherTab.Logs },
                    label = { Text("Logs") },
                    icon = { Text(LauncherTab.Logs.iconSymbol, fontSize = 18.sp) }
                )
            }
        }
    ) { innerPadding ->
        Box(
            modifier = Modifier
                .fillMaxSize()
                .padding(innerPadding)
        ) {
            when (selectedTab) {
                LauncherTab.Drivers -> {
                    DriversTabContent(
                        drivers = drivers,
                        selectedDriver = selectedDriver,
                        probeResult = probeResult,
                        isProbing = isProbing,
                        importing = importing,
                        onSelectDriver = { driver ->
                            selectedDriverId = driver.id
                            DriverManager.setSelectedDriverId(context, driver.id)
                        },
                        onImportClick = {
                            importLauncher.launch(arrayOf("application/zip", "*/*"))
                        },
                        onProbeClick = {
                            runProbe(selectedDriver)
                        },
                        onDeleteDriver = { driver ->
                            val driverName = driver.name
                            if (DriverManager.deleteDriver(context, driver)) {
                                drivers = DriverManager.getDrivers(context)
                                if (selectedDriverId == driver.id) {
                                    selectedDriverId = "bundled"
                                    DriverManager.setSelectedDriverId(context, "bundled")
                                }
                                addLog("Deleted driver: $driverName")
                            }
                        }
                    )
                }
                LauncherTab.Components -> {
                    ComponentsTabContent(
                        catalog = ContentManager.CATALOG,
                        installedList = installedComponents,
                        busy = isComponentBusy,
                        progressText = componentProgressText,
                        lastHash = lastComputedSha256,
                        onInstallLocalClick = {
                            componentImportLauncher.launch(arrayOf("*/*"))
                        },
                        onDownloadClick = { entry ->
                            downloadComponent(entry)
                        },
                        onDeleteClick = { component ->
                            deleteComponent(component)
                        }
                    )
                }
                LauncherTab.Wine -> {
                    WineTabContent(
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
                            DisplayServer.setMode(
                                context,
                                if (it) DisplayServer.Mode.BUILTIN else DisplayServer.Mode.TERMUX
                            )
                            displayRev++
                        },
                        displayRes = displayRes,
                        onCycleRes = {
                            val all = BuiltinXServer.RESOLUTIONS
                            DisplayServer.setResolution(context, all[(all.indexOf(displayRes) + 1) % all.size])
                            displayRev++
                        },
                        displayShm = displayShm,
                        onToggleShm = {
                            DisplayServer.setShm(context, it)
                            displayRev++
                        },
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
                        onStop = { stopWine() }
                    )
                }
                LauncherTab.Games -> {
                    GamesTabContent(
                        drivers = drivers,
                        busy = isWineRunning || isSettingUp,
                        onLaunch = { runShortcut(it) }
                    )
                }
                LauncherTab.Logs -> {
                    LogsTabContent(logs = logs)
                }
            }
        }
    }
}

@Composable
fun DriversTabContent(
    drivers: List<Driver>,
    selectedDriver: Driver,
    probeResult: String?,
    isProbing: Boolean,
    importing: Boolean = false,
    onSelectDriver: (Driver) -> Unit,
    onImportClick: () -> Unit,
    onProbeClick: () -> Unit,
    onDeleteDriver: (Driver) -> Unit
) {
    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        item {
            Row(
                modifier = Modifier.fillMaxWidth(),
                horizontalArrangement = Arrangement.spacedBy(8.dp)
            ) {
                Button(
                    onClick = onImportClick,
                    enabled = !importing,
                    modifier = Modifier.weight(1f)
                ) {
                    Text("Import .adpkg.zip")
                }
                Button(
                    onClick = onProbeClick,
                    enabled = !isProbing,
                    modifier = Modifier.weight(1f)
                ) {
                    Text(if (isProbing) "Probing..." else "Probe")
                }
            }
        }

        if (probeResult != null) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceVariant
                    )
                ) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        Text(
                            text = "Probe Result:",
                            fontWeight = FontWeight.Bold,
                            style = MaterialTheme.typography.labelLarge
                        )
                        Spacer(modifier = Modifier.height(4.dp))
                        SelectionContainer {
                            Text(
                                text = probeResult,
                                fontFamily = FontFamily.Monospace,
                                fontSize = 12.sp
                            )
                        }
                    }
                }
            }
        }

        item {
            Text(
                text = "Available Drivers",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(vertical = 4.dp)
            )
        }

        items(drivers, key = { it.id }) { driver ->
            val isSelected = driver.id == selectedDriver.id
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .clickable { onSelectDriver(driver) },
                colors = CardDefaults.cardColors(
                    containerColor = if (isSelected) {
                        MaterialTheme.colorScheme.primaryContainer
                    } else {
                        MaterialTheme.colorScheme.surface
                    }
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
                        onClick = { onSelectDriver(driver) }
                    )
                    Spacer(modifier = Modifier.width(8.dp))
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = driver.name,
                            fontWeight = FontWeight.Bold,
                            style = MaterialTheme.typography.bodyLarge
                        )
                        Text(
                            text = "Version: ${driver.version} | Author: ${driver.author}",
                            style = MaterialTheme.typography.bodySmall
                        )
                        if (driver.description.isNotEmpty()) {
                            Text(
                                text = driver.description,
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        Text(
                            text = driver.libPath,
                            style = MaterialTheme.typography.labelSmall,
                            fontFamily = FontFamily.Monospace,
                            color = MaterialTheme.colorScheme.onSurfaceVariant
                        )
                    }
                    if (!driver.bundled) {
                        Spacer(modifier = Modifier.width(8.dp))
                        OutlinedButton(
                            onClick = { onDeleteDriver(driver) },
                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text("Delete")
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun LogsTabContent(logs: List<String>) {
    val scrollState = rememberScrollState()
    SelectionContainer {
        Column(
            modifier = Modifier
                .fillMaxSize()
                .verticalScroll(scrollState)
                .padding(16.dp)
        ) {
            if (logs.isEmpty()) {
                Text(
                    text = "No logs yet.",
                    fontFamily = FontFamily.Monospace,
                    fontSize = 13.sp,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            } else {
                logs.forEach { entry ->
                    Text(
                        text = entry,
                        fontFamily = FontFamily.Monospace,
                        fontSize = 12.sp,
                        modifier = Modifier.padding(vertical = 4.dp)
                    )
                    HorizontalDivider(modifier = Modifier.padding(vertical = 2.dp))
                }
            }
        }
    }
}

@Composable
fun ComponentsTabContent(
    catalog: List<CatalogEntry>,
    installedList: List<InstalledContent>,
    busy: Boolean,
    progressText: String?,
    lastHash: String?,
    onInstallLocalClick: () -> Unit,
    onDownloadClick: (CatalogEntry) -> Unit,
    onDeleteClick: (InstalledContent) -> Unit
) {
    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        item {
            Button(
                onClick = onInstallLocalClick,
                enabled = !busy,
                modifier = Modifier.fillMaxWidth()
            ) {
                Text("Install local .wcp")
            }
        }

        if (progressText != null || lastHash != null) {
            item {
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surfaceVariant
                    )
                ) {
                    Column(modifier = Modifier.padding(12.dp)) {
                        if (progressText != null) {
                            Text(
                                text = progressText,
                                fontWeight = FontWeight.Bold,
                                style = MaterialTheme.typography.bodyMedium
                            )
                        }
                        if (lastHash != null) {
                            if (progressText != null) Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "SHA-256: $lastHash",
                                fontFamily = FontFamily.Monospace,
                                fontSize = 11.sp,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                }
            }
        }

        item {
            Text(
                text = "Catalog",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(vertical = 4.dp)
            )
        }

        items(catalog, key = { it.url }) { entry ->
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surface
                )
            ) {
                Row(
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Column(modifier = Modifier.weight(1f)) {
                        Text(
                            text = entry.name,
                            fontWeight = FontWeight.Bold,
                            style = MaterialTheme.typography.bodyLarge
                        )
                        Text(
                            text = "Type: ${entry.type}",
                            style = MaterialTheme.typography.bodySmall
                        )
                        val hashLabel = if (entry.sha256 != null) {
                            "sha256 pinned"
                        } else {
                            "UNVERIFIED (hash shown after download)"
                        }
                        Text(
                            text = hashLabel,
                            style = MaterialTheme.typography.labelSmall,
                            color = if (entry.sha256 != null) {
                                MaterialTheme.colorScheme.primary
                            } else {
                                MaterialTheme.colorScheme.error
                            }
                        )
                    }
                    Spacer(modifier = Modifier.width(8.dp))
                    Button(
                        onClick = { onDownloadClick(entry) },
                        enabled = !busy
                    ) {
                        Text("Download")
                    }
                }
            }
        }

        item {
            Text(
                text = "Installed Components",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(vertical = 4.dp)
            )
        }

        if (installedList.isEmpty()) {
            item {
                Text(
                    text = "No components installed.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        } else {
            items(installedList, key = { it.dir.absolutePath }) { c ->
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surface
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = "${c.type} — ${c.versionName}",
                                fontWeight = FontWeight.Bold,
                                style = MaterialTheme.typography.bodyLarge
                            )
                            if (c.description.isNotEmpty()) {
                                Text(
                                    text = c.description,
                                    style = MaterialTheme.typography.bodySmall,
                                    color = MaterialTheme.colorScheme.onSurfaceVariant
                                )
                            }
                            Text(
                                text = c.dir.absolutePath,
                                style = MaterialTheme.typography.labelSmall,
                                fontFamily = FontFamily.Monospace,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        OutlinedButton(
                            onClick = { onDeleteClick(c) },
                            enabled = !busy,
                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 4.dp)
                        ) {
                            Text("Delete")
                        }
                    }
                }
            }
        }
    }
}

@Composable
fun WineTabContent(
    isSetup: Boolean,
    isSettingUp: Boolean,
    isRunning: Boolean,
    installedComponents: List<InstalledContent>,
    recentExes: List<String>,
    selectedDriver: Driver,
    isDxvkEnabled: Boolean,
    onToggleDxvk: (Boolean) -> Unit,
    displayStatus: String,
    builtinDisplay: Boolean,
    onToggleBuiltin: (Boolean) -> Unit,
    displayRes: String,
    onCycleRes: () -> Unit,
    displayShm: Boolean,
    onToggleShm: (Boolean) -> Unit,
    onOpenDisplay: () -> Unit,
    onOpenScreen: () -> Unit,
    onLaunchExplorer: () -> Unit,
    onRunCmdVer: () -> Unit,
    onPickExe: () -> Unit,
    onRunManualExe: (String) -> Unit,
    onRunRecentExe: (String) -> Unit,
    onStop: () -> Unit
) {
    var manualPath by remember { mutableStateOf("") }

    val protonVer = installedComponents.firstOrNull { it.type == "Proton" }?.versionName ?: "Not installed"
    val fexVer = installedComponents.firstOrNull { it.type == "FEXCore" }?.versionName ?: "None"
    val imagefsVer = installedComponents.firstOrNull { it.type == "imagefs" && it.versionName == "bionic" }?.versionName ?: "Not installed"

    val actionEnabled = !isRunning && !isSettingUp && isSetup

    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp)
    ) {
        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surface
                )
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Wine Environment",
                            fontWeight = FontWeight.Bold,
                            style = MaterialTheme.typography.titleMedium
                        )
                        if (isRunning) {
                            Text(
                                text = "Running",
                                style = MaterialTheme.typography.labelSmall,
                                color = MaterialTheme.colorScheme.primary,
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }

                    if (isSettingUp) {
                        Text(
                            text = "Setting up container...",
                            fontWeight = FontWeight.SemiBold,
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.primary
                        )
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                    } else {
                        val statusText = if (isSetup) "Container: Ready" else "Container: Not set up"
                        val statusColor = if (isSetup) MaterialTheme.colorScheme.primary else MaterialTheme.colorScheme.error
                        Text(
                            text = statusText,
                            style = MaterialTheme.typography.bodyMedium,
                            color = statusColor,
                            fontWeight = FontWeight.SemiBold
                        )
                    }

                    Text(
                        text = "Proton: $protonVer | FEX: $fexVer | imagefs: $imagefsVer",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )

                    Text(
                        text = "Vulkan driver: ${selectedDriver.name} (${File(selectedDriver.libPath).name})",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Text(
                        text = displayStatus,
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "Built-in X server (off = Termux:X11)",
                            style = MaterialTheme.typography.bodyMedium
                        )
                        Switch(
                            checked = builtinDisplay,
                            onCheckedChange = onToggleBuiltin,
                            enabled = !isRunning
                        )
                    }
                    if (builtinDisplay) {
                        Row(
                            modifier = Modifier.fillMaxWidth(),
                            horizontalArrangement = Arrangement.SpaceBetween,
                            verticalAlignment = Alignment.CenterVertically
                        ) {
                            Text(
                                text = "MIT-SHM present",
                                style = MaterialTheme.typography.bodyMedium
                            )
                            Switch(
                                checked = displayShm,
                                onCheckedChange = onToggleShm,
                                enabled = !isRunning
                            )
                        }
                        Button(onClick = onCycleRes, enabled = !isRunning) {
                            Text("Resolution: $displayRes (tap to cycle)")
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "DXVK (d3d8/9/10/11 -> Vulkan)",
                            style = MaterialTheme.typography.bodyMedium
                        )
                        Switch(
                            checked = isDxvkEnabled,
                            onCheckedChange = onToggleDxvk
                        )
                    }
                    Text(
                        text = "DXVK enabled is not a game compatibility result. ARM64EC smoke tests do not validate x86/i686 WOW64 games; 32-bit staging can fail on Mali kbase SAME_VA.",
                        style = MaterialTheme.typography.bodySmall,
                        color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
            }
        }

        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surface
                )
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        text = "Actions",
                        fontWeight = FontWeight.Bold,
                        style = MaterialTheme.typography.titleMedium
                    )

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = onLaunchExplorer,
                            enabled = actionEnabled,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Launch explorer")
                        }
                        Button(
                            onClick = onRunCmdVer,
                            enabled = actionEnabled,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("wine cmd /c ver")
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = onPickExe,
                            enabled = actionEnabled,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Pick .exe")
                        }
                        Button(
                            onClick = onStop,
                            enabled = isRunning,
                            colors = ButtonDefaults.buttonColors(
                                containerColor = MaterialTheme.colorScheme.error
                            ),
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Stop")
                        }
                    }

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = onOpenDisplay,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Termux:X11")
                        }
                        OutlinedButton(
                            onClick = onOpenScreen,
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Framebuffer")
                        }
                    }
                }
            }
        }

        item {
            Card(
                modifier = Modifier.fillMaxWidth(),
                colors = CardDefaults.cardColors(
                    containerColor = MaterialTheme.colorScheme.surface
                )
            ) {
                Column(
                    modifier = Modifier.padding(16.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Text(
                        text = "Run Executable by Path",
                        fontWeight = FontWeight.Bold,
                        style = MaterialTheme.typography.titleMedium
                    )
                    OutlinedTextField(
                        value = manualPath,
                        onValueChange = { manualPath = it },
                        label = { Text("Executable Path") },
                        placeholder = { Text("/storage/emulated/0/Download/app.exe") },
                        singleLine = true,
                        modifier = Modifier.fillMaxWidth()
                    )
                    Button(
                        onClick = { onRunManualExe(manualPath.trim()) },
                        enabled = actionEnabled && manualPath.isNotBlank(),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Run")
                    }
                }
            }
        }

        item {
            Text(
                text = "Recent Executables",
                style = MaterialTheme.typography.titleMedium,
                fontWeight = FontWeight.Bold,
                modifier = Modifier.padding(vertical = 4.dp)
            )
        }

        if (recentExes.isEmpty()) {
            item {
                Text(
                    text = "No recent executables.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        } else {
            items(recentExes) { exePath ->
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable(enabled = actionEnabled) {
                            onRunRecentExe(exePath)
                        },
                    colors = CardDefaults.cardColors(
                        containerColor = MaterialTheme.colorScheme.surface
                    )
                ) {
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .padding(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Column(modifier = Modifier.weight(1f)) {
                            Text(
                                text = File(exePath).name,
                                fontWeight = FontWeight.Bold,
                                style = MaterialTheme.typography.bodyMedium
                            )
                            Text(
                                text = exePath,
                                style = MaterialTheme.typography.labelSmall,
                                fontFamily = FontFamily.Monospace,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                        Spacer(modifier = Modifier.width(8.dp))
                        Button(
                            onClick = { onRunRecentExe(exePath) },
                            enabled = actionEnabled
                        ) {
                            Text("Run")
                        }
                    }
                }
            }
        }
    }
}
