package dev.zenithblue.panvklauncher

import android.graphics.BitmapFactory
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.ExperimentalFoundationApi
import androidx.compose.foundation.Image
import androidx.compose.foundation.combinedClickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
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
import androidx.compose.ui.unit.sp
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File

private val ARCHES = listOf("auto", "i386", "x86_64", "arm64ec")

/** Game library: tap = launch (via [onLaunch], the app's normal launch path), long-press / menu = edit, duplicate, delete. */
@OptIn(ExperimentalFoundationApi::class)
@Composable
fun GamesTabContent(
    drivers: List<Driver>,
    busy: Boolean,
    onLaunch: (Shortcut) -> Unit
) {
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()
    var games by remember { mutableStateOf(ShortcutStore.list(ctx)) }
    var editing by remember { mutableStateOf<Shortcut?>(null) }
    var editingIsNew by remember { mutableStateOf(false) }
    var menuFor by remember { mutableStateOf<String?>(null) }
    var deleting by remember { mutableStateOf<Shortcut?>(null) }

    fun refresh() { games = ShortcutStore.list(ctx) }

    // Shortcuts written over adb have arch/icon = auto: resolve them here.
    LaunchedEffect(games) {
        if (games.any { it.arch == "auto" || it.icon == "auto" }) {
            withContext(Dispatchers.IO) { games.forEach { ShortcutStore.ensureMeta(ctx, it) } }
            refresh()
        }
    }

    Column(Modifier.fillMaxSize().padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
        Button(
            onClick = {
                editingIsNew = true
                editing = Shortcut(id = ShortcutStore.newId(), name = "", exe = "")
            },
            modifier = Modifier.fillMaxWidth().heightIn(min = 48.dp)
                .semantics { contentDescription = "Add game" }
        ) { Text("Add game") }

        if (games.isEmpty()) {
            Text(
                "No games yet. Tap Add game to pick a Windows .exe.",
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }

        LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.weight(1f)) {
            items(games, key = { it.id }) { g ->
                Card(
                    modifier = Modifier.fillMaxWidth().combinedClickable(
                        enabled = !busy,
                        onClickLabel = "Launch ${g.name}",
                        onClick = { onLaunch(g) },
                        onLongClickLabel = "Game options",
                        onLongClick = { menuFor = g.id }
                    ).semantics { contentDescription = "${g.name}, ${g.arch}. Tap to launch, long press for options." },
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surface)
                ) {
                    Row(
                        Modifier.fillMaxWidth().padding(12.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        GameIcon(g)
                        Spacer(Modifier.width(12.dp))
                        Column(Modifier.weight(1f)) {
                            Text(g.name, fontWeight = FontWeight.Bold, style = MaterialTheme.typography.bodyLarge)
                            Text(
                                listOf(g.arch, g.resolution.ifEmpty { "default res" }, g.driver.ifEmpty { "default driver" })
                                    .joinToString(" | "),
                                style = MaterialTheme.typography.bodySmall
                            )
                            Text(
                                g.exe, style = MaterialTheme.typography.labelSmall, fontFamily = FontFamily.Monospace,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                                maxLines = 1, overflow = TextOverflow.Ellipsis
                            )
                        }
                        Box {
                            TextButton(
                                onClick = { menuFor = g.id },
                                modifier = Modifier.sizeIn(minWidth = 48.dp, minHeight = 48.dp)
                                    .semantics { contentDescription = "Options for ${g.name}" }
                            ) { Text("⋮", fontSize = 20.sp) }
                            DropdownMenu(expanded = menuFor == g.id, onDismissRequest = { menuFor = null }) {
                                DropdownMenuItem(text = { Text("Edit") }, onClick = {
                                    menuFor = null; editingIsNew = false; editing = g
                                })
                                DropdownMenuItem(text = { Text("Duplicate") }, onClick = {
                                    menuFor = null
                                    scope.launch(Dispatchers.IO) { ShortcutStore.duplicate(ctx, g); withContext(Dispatchers.Main) { refresh() } }
                                })
                                DropdownMenuItem(text = { Text("Delete") }, onClick = {
                                    menuFor = null; deleting = g
                                })
                            }
                        }
                    }
                }
            }
        }
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
        ShortcutEditor(
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

@Composable
private fun GameIcon(g: Shortcut) {
    val ctx = LocalContext.current
    val bmp = remember(g.id, g.icon) {
        ShortcutStore.iconFile(ctx, g)?.let { BitmapFactory.decodeFile(it.path)?.asImageBitmap() }
    }
    if (bmp != null) {
        Image(bmp, contentDescription = null, modifier = Modifier.size(56.dp).clip(RoundedCornerShape(8.dp)))
    } else {
        Surface(
            color = MaterialTheme.colorScheme.primaryContainer,
            shape = RoundedCornerShape(8.dp), modifier = Modifier.size(56.dp)
        ) {
            Box(contentAlignment = Alignment.Center) {
                Text(
                    g.name.firstOrNull()?.uppercase() ?: "?", fontSize = 24.sp, fontWeight = FontWeight.Bold,
                    color = MaterialTheme.colorScheme.onPrimaryContainer
                )
            }
        }
    }
}

@Composable
private fun ShortcutEditor(
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
    var driver by remember { mutableStateOf(initial.driver) }
    var importing by remember { mutableStateOf(false) }
    var detected by remember { mutableStateOf<String?>(null) }

    LaunchedEffect(exe) {
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

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(if (isNew) "Add game" else "Edit game") },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                OutlinedTextField(name, { name = it }, label = { Text("Name") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(
                    exe, { exe = it }, label = { Text("Executable (device path or C:\\...)") },
                    modifier = Modifier.fillMaxWidth(),
                    supportingText = { Text(if (importing) "Importing..." else detected?.let { "Detected: $it" } ?: "Detected: unknown / not found") }
                )
                OutlinedButton(
                    onClick = { picker.launch(arrayOf("*/*")) }, enabled = !importing,
                    modifier = Modifier.heightIn(min = 48.dp)
                ) { Text("Browse for .exe") }
                OutlinedTextField(args, { args = it }, label = { Text("Arguments") }, singleLine = true, modifier = Modifier.fillMaxWidth())
                OutlinedTextField(
                    env, { env = it }, label = { Text("Environment (KEY=VALUE per line)") },
                    textStyle = MaterialTheme.typography.bodySmall.copy(fontFamily = FontFamily.Monospace),
                    minLines = 2, modifier = Modifier.fillMaxWidth()
                )
                ChipRow("Architecture", ARCHES.map { it to it }, arch) { arch = it }
                ChipRow(
                    "Resolution",
                    listOf("" to "default") + BuiltinXServer.RESOLUTIONS.map { it to it }, res
                ) { res = it }
                ChipRow(
                    "Driver",
                    listOf("" to "default") + drivers.map { it.id to it.name }, driver
                ) { driver = it }
            }
        },
        confirmButton = {
            TextButton(
                enabled = exe.isNotBlank() && !importing,
                onClick = {
                    val n = name.ifBlank { File(exe.replace('\\', '/')).nameWithoutExtension }
                    onSave(
                        initial.copy(
                            name = n, exe = exe.trim(), args = args.trim(), env = ShortcutStore.parseEnv(env),
                            arch = arch, resolution = res, driver = driver,
                            // exe changed: re-extract icon
                            icon = if (exe.trim() != initial.exe) "auto" else initial.icon
                        )
                    )
                },
                modifier = Modifier.heightIn(min = 48.dp)
            ) { Text("Save") }
        },
        dismissButton = { TextButton(onClick = onDismiss, modifier = Modifier.heightIn(min = 48.dp)) { Text("Cancel") } }
    )
}

@Composable
private fun ChipRow(label: String, options: List<Pair<String, String>>, selected: String, onSelect: (String) -> Unit) {
    Text(label, style = MaterialTheme.typography.labelLarge)
    Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
        options.forEach { (value, text) ->
            FilterChip(
                selected = selected == value, onClick = { onSelect(value) },
                label = { Text(text) },
                modifier = Modifier.heightIn(min = 48.dp).semantics { contentDescription = "$label $text" }
            )
        }
    }
}
