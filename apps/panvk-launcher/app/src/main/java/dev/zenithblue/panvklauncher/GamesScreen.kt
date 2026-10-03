package dev.zenithblue.panvklauncher

import android.graphics.BitmapFactory
import android.text.format.DateUtils
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.MoreVert
import androidx.compose.material.icons.rounded.ContentCopy
import androidx.compose.material.icons.rounded.DeleteOutline
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Stop
import androidx.compose.material3.FilledIconButton
import androidx.compose.animation.AnimatedVisibility
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

private val ARCHES = listOf("auto", "i386", "x86_64", "arm64ec")

/** Game library. Tap a game (or Play) = Launch card; the card's Launch button starts it via [onLaunch]. */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun GamesScreen(
    drivers: List<Driver>,
    activeDriver: Driver,
    defaultResolution: String,
    busy: Boolean,
    runningName: String?,
    onStop: () -> Unit,
    onLaunch: (Shortcut) -> Unit
) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var games by remember { mutableStateOf(ShortcutStore.list(ctx)) }
    var editing by remember { mutableStateOf<Shortcut?>(null) }
    var editingIsNew by remember { mutableStateOf(false) }
    var menuFor by remember { mutableStateOf<String?>(null) }
    var deleting by remember { mutableStateOf<Shortcut?>(null) }
    var launching by remember { mutableStateOf<Shortcut?>(null) }

    fun refresh() { games = ShortcutStore.list(ctx) }

    // Shortcuts written over adb have arch/icon = auto: resolve them here.
    LaunchedEffect(games) {
        if (games.any { it.arch == "auto" || it.icon == "auto" }) {
            withContext(Dispatchers.IO) { games.forEach { ShortcutStore.ensureMeta(ctx, it) } }
            refresh()
        }
    }

    // Games whose exe matches a controller preset (e.g. MiSide.exe) get that preset as their own editable file.
    LaunchedEffect(games) {
        withContext(Dispatchers.IO) { games.forEach { ControllerConfig.attachPreset(ctx, it.id, ShortcutStore.resolveExe(ctx, it.exe)) } }
    }

    Box(Modifier.fillMaxSize()) {
        Column(Modifier.fillMaxSize()) {
            AnimatedVisibility(visible = busy) { RunningBanner(runningName, onStop) }
            if (games.isEmpty()) {
                Box(Modifier.fillMaxSize().verticalScroll(rememberScrollState()), contentAlignment = Alignment.Center) {
                    EmptyState(
                        icon = Icons.Rounded.SportsEsports,
                        title = "No games yet",
                        body = "Add a Windows .exe to build your library, then tap it to launch.",
                        modifier = Modifier.widthIn(max = 420.dp),
                        action = {
                            Button(onClick = {
                                editingIsNew = true
                                editing = Shortcut(id = ShortcutStore.newId(), name = "", exe = "")
                            }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Add game") }
                        }
                    )
                }
            } else {
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(minSize = 400.dp),
                    modifier = Modifier.fillMaxSize(),
                    // Bottom padding keeps the last row clear of the FAB.
                    contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 8.dp, bottom = 96.dp),
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    items(games, key = { it.id }) { g ->
                        GameCard(
                            g = g,
                            busy = busy,
                            defaultResolution = defaultResolution,
                            menuOpen = menuFor == g.id,
                            onOpenMenu = { menuFor = g.id },
                            onCloseMenu = { menuFor = null },
                            onPlay = { launching = g },
                            onEdit = { menuFor = null; editingIsNew = false; editing = g },
                            onDuplicate = {
                                menuFor = null
                                scope.launch(Dispatchers.IO) { ShortcutStore.duplicate(ctx, g); withContext(Dispatchers.Main) { refresh() } }
                            },
                            onDelete = { menuFor = null; deleting = g }
                        )
                    }
                }
            }
        }
        if (games.isNotEmpty()) {
            ExtendedFloatingActionButton(
                onClick = {
                    editingIsNew = true
                    editing = Shortcut(id = ShortcutStore.newId(), name = "", exe = "")
                },
                icon = { Icon(Icons.Rounded.Add, contentDescription = null) },
                text = { Text("Add game") },
                modifier = Modifier
                    .align(Alignment.BottomEnd)
                    .padding(16.dp)
                    .semantics { contentDescription = "Add game" }
            )
        }
    }

    launching?.let { g ->
        LaunchSheet(
            game = g,
            drivers = drivers,
            activeDriver = activeDriver,
            defaultResolution = defaultResolution,
            busy = busy,
            onDismiss = { launching = null },
            onLaunch = { updated ->
                launching = null
                // Resolution / driver picked on the card are saved on the game, then it starts.
                scope.launch(Dispatchers.IO) {
                    ShortcutStore.save(ctx, updated)
                    withContext(Dispatchers.Main) { refresh(); onLaunch(updated) }
                }
            }
        )
    }

    deleting?.let { g ->
        AlertDialog(
            onDismissRequest = { deleting = null },
            title = { Text("Delete ${g.name}?") },
            text = { Text("Removes the shortcut only. The game files stay.") },
            confirmButton = {
                TextButton(onClick = { ShortcutStore.delete(ctx, g); deleting = null; refresh() }) { Text("Delete") }
            },
            dismissButton = { TextButton(onClick = { deleting = null }) { Text("Cancel") } }
        )
    }

    editing?.let { e ->
        ShortcutEditorSheet(
            initial = e, isNew = editingIsNew, drivers = drivers,
            onDismiss = { editing = null },
            onSave = { s ->
                scope.launch(Dispatchers.IO) {
                    ShortcutStore.save(ctx, ShortcutStore.ensureMeta(ctx, s))
                    withContext(Dispatchers.Main) { editing = null; refresh() }
                }
            }
        )
    }
}

/** Shown on top of the library while Wine runs: which game, and a Stop button (EXIT in-game does the same). */
@Composable
private fun RunningBanner(name: String?, onStop: () -> Unit) {
    val cs = MaterialTheme.colorScheme
    Card(
        modifier = Modifier.fillMaxWidth().padding(start = 16.dp, end = 16.dp, top = 8.dp),
        colors = CardDefaults.cardColors(containerColor = cs.tertiaryContainer, contentColor = cs.onTertiaryContainer)
    ) {
        Row(Modifier.padding(start = 16.dp, end = 8.dp, top = 6.dp, bottom = 6.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Rounded.SportsEsports, contentDescription = null)
            Spacer(Modifier.width(12.dp))
            Column(Modifier.weight(1f)) {
                Text("Running", style = MaterialTheme.typography.labelMedium)
                Text(name ?: "Wine", style = MaterialTheme.typography.titleSmall, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            TextButton(onClick = onStop, modifier = Modifier.heightIn(min = 48.dp)) {
                Icon(Icons.Rounded.Stop, contentDescription = null)
                Spacer(Modifier.width(6.dp))
                Text("Stop")
            }
        }
    }
}

@Composable
private fun GameCard(
    g: Shortcut,
    busy: Boolean,
    defaultResolution: String,
    menuOpen: Boolean,
    onOpenMenu: () -> Unit,
    onCloseMenu: () -> Unit,
    onPlay: () -> Unit,
    onEdit: () -> Unit,
    onDuplicate: () -> Unit,
    onDelete: () -> Unit
) {
    val played = if (g.lastPlayed > 0)
        "Played " + DateUtils.getRelativeTimeSpanString(
            g.lastPlayed, System.currentTimeMillis(), DateUtils.MINUTE_IN_MILLIS, DateUtils.FORMAT_ABBREV_RELATIVE
        )
    else "Never played"
    val res = g.resolution.ifEmpty { defaultResolution }
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .clickable(enabled = !busy, onClickLabel = "Open launch card for ${g.name}", onClick = onPlay),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow),
        shape = MaterialTheme.shapes.large
    ) {
        Row(Modifier.padding(12.dp), verticalAlignment = Alignment.CenterVertically) {
            GameIcon(g, size = 72)
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                Text(g.name, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold, maxLines = 2, overflow = TextOverflow.Ellipsis)
                Text(
                    "${g.arch}  ·  $res",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 1, overflow = TextOverflow.Ellipsis
                )
                Text(played, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            Box {
                IconButton(onClick = onOpenMenu, modifier = Modifier.size(48.dp)) {
                    Icon(Icons.Rounded.MoreVert, contentDescription = "Options for ${g.name}")
                }
                DropdownMenu(expanded = menuOpen, onDismissRequest = onCloseMenu) {
                    DropdownMenuItem(text = { Text("Edit") }, onClick = onEdit, leadingIcon = { Icon(Icons.Rounded.Edit, null) }, modifier = Modifier.heightIn(min = 48.dp))
                    DropdownMenuItem(text = { Text("Duplicate") }, onClick = onDuplicate, leadingIcon = { Icon(Icons.Rounded.ContentCopy, null) }, modifier = Modifier.heightIn(min = 48.dp))
                    DropdownMenuItem(text = { Text("Delete") }, onClick = onDelete, leadingIcon = { Icon(Icons.Rounded.DeleteOutline, null) }, modifier = Modifier.heightIn(min = 48.dp))
                }
            }
            // Same action as tapping the card: opens the launch card (nothing starts without it).
            FilledIconButton(onClick = onPlay, enabled = !busy, modifier = Modifier.size(52.dp)) {
                Icon(Icons.Rounded.PlayArrow, contentDescription = if (busy) "Wine is running" else "Launch ${g.name}", modifier = Modifier.size(28.dp))
            }
        }
    }
}

@Composable
fun GameIcon(g: Shortcut, size: Int = 56) {
    val ctx = LocalContext.current
    val cs = MaterialTheme.colorScheme
    val bmp = remember(g.id, g.icon) {
        ShortcutStore.iconFile(ctx, g)?.let { BitmapFactory.decodeFile(it.path)?.asImageBitmap() }
    }
    // Restrained fallback palette: the three theme container colors, picked by name so a game keeps its color.
    val (bg, fg) = when (Math.floorMod(g.name.hashCode(), 3)) {
        0 -> cs.primaryContainer to cs.onPrimaryContainer
        1 -> cs.secondaryContainer to cs.onSecondaryContainer
        else -> cs.tertiaryContainer to cs.onTertiaryContainer
    }
    Surface(
        color = if (bmp != null) cs.surfaceContainerHighest else bg,
        shape = RoundedCornerShape((size / 5).dp), modifier = Modifier.size(size.dp)
    ) {
        Box(contentAlignment = Alignment.Center) {
            if (bmp != null) {
                // Exe icons are 32-48 px: keep them crisp and inset on a tile instead of blowing them up.
                Image(bmp, contentDescription = null, filterQuality = FilterQuality.Low, modifier = Modifier.fillMaxSize().padding((size / 8).dp))
            } else {
                Text(
                    g.name.firstOrNull()?.uppercase() ?: "?",
                    style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Bold, color = fg
                )
            }
        }
    }
}

@OptIn(ExperimentalMaterial3Api::class)
@Composable
private fun ShortcutEditorSheet(
    initial: Shortcut,
    isNew: Boolean,
    drivers: List<Driver>,
    onDismiss: () -> Unit,
    onSave: (Shortcut) -> Unit
) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var name by remember { mutableStateOf(initial.name) }
    var exe by remember { mutableStateOf(initial.exe) }
    var args by remember { mutableStateOf(initial.args) }
    var env by remember { mutableStateOf(ShortcutStore.envText(initial.env)) }
    var arch by remember { mutableStateOf(initial.arch) }
    var res by remember { mutableStateOf(initial.resolution) }
    var driver by remember { mutableStateOf(DriverManager.find(ctx, drivers, initial.driver)?.id ?: "") }
    var importing by remember { mutableStateOf(false) }
    var detected by remember { mutableStateOf<String?>(null) }
    var editCtl by remember { mutableStateOf(false) }
    var ctlRev by remember { mutableStateOf(0) }
    var ctlRef by remember { mutableStateOf(ControllerLibrary.assignment(ctx, initial.id)) }
    val ctlOptions = remember(ctlRev) {
        buildList {
            add("" to "Automatic (exe template or Default)")
            if (ControllerLibrary.assignment(ctx, initial.id) == "own") add("own" to "This game's own config")
            ControllerLibrary.templates(ctx).forEach { add(it.ref to "${it.config.name} (template)") }
            ControllerLibrary.user(ctx).forEach { add(it.ref to it.config.name) }
        }
    }
    if (editCtl) {
        GameControllerEditor(initial.id, name.ifBlank { "this game" }, ShortcutStore.resolveExe(ctx, exe)) {
            editCtl = false; ctlRef = ControllerLibrary.assignment(ctx, initial.id); ctlRev++
        }
    }

    LaunchedEffect(exe) {
        // New executable: the previous one's architecture no longer applies.
        if (exe.trim() != initial.exe) arch = "auto"
        detected = withContext(Dispatchers.IO) { PeInfo.arch(ShortcutStore.resolveExe(ctx, exe)) }
    }

    val picker = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) {
            importing = true
            scope.launch(Dispatchers.IO) {
                val path = ContainerManager.importUri(ctx, uri)
                withContext(Dispatchers.Main) {
                    importing = false
                    if (path != null) {
                        exe = path
                        if (name.isBlank()) name = File(path).nameWithoutExtension
                    }
                }
            }
        }
    }

    ModalBottomSheet(onDismissRequest = onDismiss, sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true)) {
        Column(
            modifier = Modifier
                .fillMaxWidth()
                .verticalScroll(rememberScrollState())
                .padding(horizontal = 20.dp)
                .navigationBarsPadding(),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Text(if (isNew) "Add game" else "Edit game", style = MaterialTheme.typography.titleLarge)
            OutlinedTextField(name, { name = it }, label = { Text("Name") }, singleLine = true, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                exe, { exe = it }, label = { Text("Executable (device path or C:\\...)") },
                modifier = Modifier.fillMaxWidth(),
                supportingText = { Text(if (importing) "Importing..." else detected?.let { "Detected: $it" } ?: "Detected: unknown / not found") }
            )
            OutlinedButton(onClick = { picker.launch(arrayOf("*/*")) }, enabled = !importing, modifier = Modifier.heightIn(min = 48.dp)) {
                Text("Browse for .exe")
            }
            OutlinedTextField(args, { args = it }, label = { Text("Arguments") }, singleLine = true, modifier = Modifier.fillMaxWidth())
            OutlinedTextField(
                env, { env = it }, label = { Text("Environment (KEY=VALUE per line)") },
                textStyle = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace),
                minLines = 2, modifier = Modifier.fillMaxWidth()
            )
            DropdownField("Architecture", ARCHES.map { it to it }, arch, { arch = it })
            ResolutionField("Resolution", res, { res = it }, defaultLabel = "Launcher default")
            DropdownField(
                "Driver",
                listOf("" to "Launcher default") + drivers.map { it.id to it.label }, driver, { driver = it }
            )
            DropdownField("Controller config", ctlOptions, ctlRef, { ctlRef = it })
            OutlinedButton(
                // Editor works on the saved assignment: apply the picked one first.
                onClick = { ControllerLibrary.assign(ctx, initial.id, ctlRef); editCtl = true },
                modifier = Modifier.heightIn(min = 48.dp).semantics { contentDescription = "Edit controller config" }
            ) { Text("Edit controls…") }

            Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = onDismiss, modifier = Modifier.heightIn(min = 48.dp)) { Text("Cancel") }
                Button(
                    enabled = exe.isNotBlank() && !importing,
                    onClick = {
                        val n = name.ifBlank { File(exe.replace('\\', '/')).nameWithoutExtension }
                        if (ctlRef != ControllerLibrary.assignment(ctx, initial.id)) ControllerLibrary.assign(ctx, initial.id, ctlRef)
                        onSave(
                            initial.copy(
                                name = n, exe = exe.trim(), args = args.trim(), env = ShortcutStore.parseEnv(env),
                                arch = arch, resolution = res, driver = driver,
                                // exe changed: re-extract icon
                                icon = if (exe.trim() != initial.exe) "auto" else initial.icon
                            )
                        )
                    },
                    modifier = Modifier.weight(1f).heightIn(min = 48.dp)
                ) { Text("Save") }
            }
            Spacer(Modifier.height(16.dp))
        }
    }
}
