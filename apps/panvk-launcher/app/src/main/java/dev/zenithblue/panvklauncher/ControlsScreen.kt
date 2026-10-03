// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.ContentCopy
import androidx.compose.material.icons.rounded.DeleteOutline
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.FileDownload
import androidx.compose.material.icons.rounded.FileUpload
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.MoreVert
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import org.json.JSONObject

/** Controller config library: read-only templates, user configs (new / edit / duplicate / delete / import / export). */
@OptIn(ExperimentalLayoutApi::class)
@Composable
fun ControlsScreen(onLog: (String) -> Unit) {
    val ctx = LocalContext.current
    var rev by remember { mutableStateOf(0) }
    val templates = remember(rev) { ControllerLibrary.templates(ctx) }
    val mine = remember(rev) { ControllerLibrary.user(ctx) }
    var editing by remember { mutableStateOf<ControllerLibrary.Entry?>(null) }
    var deleting by remember { mutableStateOf<ControllerLibrary.Entry?>(null) }
    var exporting by remember { mutableStateOf<ControllerConfig?>(null) }
    var error by remember { mutableStateOf<String?>(null) }

    fun clone(e: ControllerLibrary.Entry): ControllerLibrary.Entry {
        val c = e.config.with(name = ControllerLibrary.copyName(ctx, e.config.name), exe = emptyList())
        val ref = ControllerLibrary.save(ctx, null, c)
        rev++
        return ControllerLibrary.Entry(ref, c, false)
    }

    val importer = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri == null) return@rememberLauncherForActivityResult
        try {
            val text = ctx.contentResolver.openInputStream(uri)!!.use { s -> s.readBytes().also { if (it.size > 256 * 1024) throw IllegalArgumentException("file too large") }.decodeToString() }
            val j = JSONObject(text)
            require(j.has("bindings") || j.has("leftStick") || j.has("rightStick")) { "no bindings / sticks" }
            val c = ControllerConfig.fromJson(j)
            val named = c.with(name = if (ControllerLibrary.all(ctx).any { it.config.name == c.name }) ControllerLibrary.copyName(ctx, c.name) else c.name, exe = emptyList())
            ControllerLibrary.save(ctx, null, named)
            onLog("Imported controller config '${named.name}'")
            rev++
        } catch (t: Throwable) {
            error = "Import failed: not a controller config (${t.message})"
        }
    }
    val exporter = rememberLauncherForActivityResult(ActivityResultContracts.CreateDocument("application/json")) { uri ->
        val c = exporting; exporting = null
        if (uri == null || c == null) return@rememberLauncherForActivityResult
        try {
            ctx.contentResolver.openOutputStream(uri)!!.use { it.write(c.toJson().toString(2).toByteArray()) }
            onLog("Exported controller config '${c.name}'")
        } catch (t: Throwable) { error = "Export failed: ${t.message}" }
    }

    PageList {
        item {
            Text(
                "Controller configs map the on-screen pad and physical controllers to keys and mouse. Assign one to a game in its Edit sheet.",
                style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        item {
            FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Button(onClick = { templates.firstOrNull { it.ref == "preset:default.json" }?.let { editing = clone(it) } }, modifier = Modifier.heightIn(min = 48.dp)) {
                    Icon(Icons.Rounded.Add, null, Modifier.size(18.dp)); Spacer(Modifier.width(8.dp)); Text("New config")
                }
                OutlinedButton(onClick = { importer.launch(arrayOf("application/json", "*/*")) }, modifier = Modifier.heightIn(min = 48.dp)) {
                    Icon(Icons.Rounded.FileUpload, null, Modifier.size(18.dp)); Spacer(Modifier.width(8.dp)); Text("Import JSON")
                }
            }
        }
        error?.let { msg -> item { ErrorBanner(msg, { error = null }) } }

        item { SectionTitle("My configs") }
        if (mine.isEmpty()) {
            item {
                Text(
                    "None yet. Clone a template below or tap New config.",
                    style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }
        items(mine, key = { it.ref }) { e ->
            ConfigCard(e,
                onOpen = { editing = e },
                onDuplicate = { clone(e) },
                onExport = { exporting = e.config; exporter.launch("${e.config.name.replace(Regex("[^A-Za-z0-9._-]"), "_")}.json") },
                onDelete = { deleting = e })
        }

        item { SectionTitle("Templates (read-only)") }
        items(templates, key = { it.ref }) { e ->
            ConfigCard(e,
                onOpen = { editing = e },
                onDuplicate = { editing = clone(e) },
                onExport = { exporting = e.config; exporter.launch("${e.config.name.replace(Regex("[^A-Za-z0-9._-]"), "_")}.json") },
                onDelete = null)
        }
    }

    editing?.let { e ->
        key(e.ref) {
            ControllerEditorScreen(
                initial = e.config,
                readOnly = e.template,
                subtitle = if (e.template) "Built-in template" else "Saved on this device",
                onDismiss = { editing = null },
                onSave = { c -> ControllerLibrary.save(ctx, e.ref, c); editing = null; rev++ },
                onClone = { editing = clone(e) }
            )
        }
    }
    deleting?.let { e ->
        AlertDialog(
            onDismissRequest = { deleting = null },
            title = { Text("Delete ${e.config.name}?") },
            text = { Text("Games using it fall back to their exe template or the default config.") },
            confirmButton = { TextButton(onClick = { ControllerLibrary.delete(ctx, e.ref); deleting = null; rev++ }) { Text("Delete") } },
            dismissButton = { TextButton(onClick = { deleting = null }) { Text("Cancel") } }
        )
    }
}

@Composable
private fun ConfigCard(
    e: ControllerLibrary.Entry,
    onOpen: () -> Unit,
    onDuplicate: () -> Unit,
    onExport: () -> Unit,
    onDelete: (() -> Unit)?
) {
    var menu by remember { mutableStateOf(false) }
    Card(onClick = onOpen, colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow), modifier = Modifier.fillMaxWidth()) {
        Row(Modifier.padding(start = 16.dp, top = 12.dp, bottom = 12.dp, end = 4.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f), verticalArrangement = Arrangement.spacedBy(4.dp)) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Text(e.config.name, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold, maxLines = 1, overflow = TextOverflow.Ellipsis, modifier = Modifier.weight(1f, fill = false))
                    if (e.template) {
                        Spacer(Modifier.width(8.dp))
                        Icon(Icons.Rounded.Lock, contentDescription = "Read-only template", modifier = Modifier.size(16.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                Text(e.config.summary(), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2)
                if (e.config.exe.isNotEmpty()) {
                    Text("Auto for ${e.config.exe.joinToString(", ")}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            }
            if (e.template) {
                TextButton(onClick = onDuplicate, modifier = Modifier.heightIn(min = 48.dp)) { Text("Clone") }
            }
            Box {
                IconButton(onClick = { menu = true }) { Icon(Icons.Rounded.MoreVert, contentDescription = "Options for ${e.config.name}") }
                DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                    DropdownMenuItem(text = { Text(if (e.template) "View" else "Edit") }, leadingIcon = { Icon(if (e.template) Icons.Rounded.Visibility else Icons.Rounded.Edit, null) }, onClick = { menu = false; onOpen() })
                    DropdownMenuItem(text = { Text(if (e.template) "Clone" else "Duplicate") }, leadingIcon = { Icon(Icons.Rounded.ContentCopy, null) }, onClick = { menu = false; onDuplicate() })
                    DropdownMenuItem(text = { Text("Export JSON") }, leadingIcon = { Icon(Icons.Rounded.FileDownload, null) }, onClick = { menu = false; onExport() })
                    if (onDelete != null) DropdownMenuItem(text = { Text("Delete") }, leadingIcon = { Icon(Icons.Rounded.DeleteOutline, null) }, onClick = { menu = false; onDelete() })
                }
            }
        }
    }
}
