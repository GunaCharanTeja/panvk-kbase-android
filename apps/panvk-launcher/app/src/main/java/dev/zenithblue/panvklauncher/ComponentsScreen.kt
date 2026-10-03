package dev.zenithblue.panvklauncher

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.CloudDownload
import androidx.compose.material.icons.rounded.DeleteOutline
import androidx.compose.material.icons.rounded.FileUpload
import androidx.compose.material.icons.rounded.Inventory2
import androidx.compose.material.icons.rounded.SystemUpdateAlt
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalContext
import androidx.compose.foundation.layout.heightIn
import androidx.compose.material3.ButtonDefaults
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp

/** "11.0-2-arm64ec" -> "v11.0-2-arm64ec", "bionic" -> "bionic". */
fun versionLabel(v: String): String = if (v.firstOrNull()?.isDigit() == true) "v$v" else v

@Composable
fun ComponentsScreen(
    catalog: List<CatalogEntry>,
    installedList: List<InstalledContent>,
    busy: Boolean,
    busyEntryType: String?,
    progressText: String?,
    errorText: String?,
    onDismissError: () -> Unit,
    onInstallLocalClick: () -> Unit,
    onDownload: (ComponentState) -> Unit,
    onDelete: (InstalledContent) -> Unit
) {
    var confirmDelete by remember { mutableStateOf<InstalledContent?>(null) }
    val context = LocalContext.current
    val bundledTypes = remember(catalog) {
        catalog.filter { ContentManager.bundledAsset(context, it) != null }.map { it.type }.toSet()
    }
    val states = remember(catalog, installedList) { catalog.map { ContentManager.stateFor(it, installedList) } }
    val catalogTypes = remember(catalog) { catalog.map { it.type }.toSet() }
    val others = remember(installedList, catalogTypes) { installedList.filter { it.type !in catalogTypes } }

    PageList {
        item {
            Text(
                "Rootfs, Wine, FEX and DXVK that games run on. Already installed components are detected from the files on this device.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        if (errorText != null) item { ErrorBanner(errorText, onDismissError) }
        if (progressText != null) item { BusyCard(progressText) }

        items(states, key = { it.entry.type }) { st ->
            ComponentCard(
                st = st,
                bundled = st.entry.type in bundledTypes,
                busy = busy,
                working = busyEntryType == st.entry.type,
                onDownload = { onDownload(st) },
                onDelete = { confirmDelete = it }
            )
        }

        if (others.isNotEmpty()) {
            item { SectionTitle("Other installed components") }
            items(others, key = { it.dir.absolutePath }) { c ->
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)
                ) {
                    Column(Modifier.padding(start = 16.dp, top = 8.dp, bottom = 8.dp, end = 8.dp)) {
                        InstalledRow(c, busy) { confirmDelete = c }
                    }
                }
            }
        }

        item { SectionTitle("Add from file") }
        item {
            FilledTonalButton(onClick = onInstallLocalClick, enabled = !busy) {
                Icon(Icons.Rounded.FileUpload, contentDescription = null, modifier = Modifier.size(18.dp))
                Spacer(Modifier.width(8.dp))
                Text("Install local .wcp")
            }
        }
        if (catalog.isEmpty() && installedList.isEmpty()) {
            item {
                EmptyState(Icons.Rounded.Inventory2, "No components", "Download one above or install a .wcp file.")
            }
        }
    }

    confirmDelete?.let { c ->
        AlertDialog(
            onDismissRequest = { confirmDelete = null },
            title = { Text("Remove ${c.type} ${versionLabel(c.versionName)}?") },
            text = { Text("Its files are deleted from this device. Games that need it will not start until it is installed again.") },
            confirmButton = { TextButton(onClick = { confirmDelete = null; onDelete(c) }) { Text("Remove") } },
            dismissButton = { TextButton(onClick = { confirmDelete = null }) { Text("Cancel") } }
        )
    }
}

@Composable
private fun ComponentCard(
    st: ComponentState,
    bundled: Boolean,
    busy: Boolean,
    working: Boolean,
    onDownload: () -> Unit,
    onDelete: (InstalledContent) -> Unit
) {
    val e = st.entry
    val tag = if (bundled && st.installedVersion == e.versionName) " (bundled)" else ""
    val verb = if (bundled) "Install" else "Download"
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(10.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Column(Modifier.weight(1f)) {
                    Text(e.title, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                    if (e.note.isNotEmpty()) {
                        Text(e.note, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
                Spacer(Modifier.width(8.dp))
                when (st.status) {
                    ComponentStatus.Installed -> StatusPill("Installed ${versionLabel(st.installedVersion ?: "")}$tag", Tone.Ok)
                    ComponentStatus.UpdateAvailable -> StatusPill("Installed ${versionLabel(st.installedVersion ?: "")}", Tone.Warn)
                    ComponentStatus.Incomplete -> StatusPill("Incomplete", Tone.Error)
                    ComponentStatus.NotInstalled -> StatusPill("Not installed")
                }
            }

            // What is on disk (one line per installed version); description comes from its profile.json.
            st.installed.forEach { c ->
                val complete = ContentManager.isComplete(c)
                Text(
                    (if (st.installed.size > 1 || !complete) "${versionLabel(c.versionName)}${if (!complete) " (incomplete)" else ""}: " else "") +
                        c.description.ifEmpty { "${c.type} ${c.versionName}" },
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 3, overflow = TextOverflow.Ellipsis
                )
            }
            if (st.status == ComponentStatus.NotInstalled) {
                Text(e.name, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            if (st.status == ComponentStatus.Incomplete) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Rounded.Warning, contentDescription = null, tint = MaterialTheme.colorScheme.error, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("Files are missing or damaged. Reinstall to repair.", style = MaterialTheme.typography.bodyMedium)
                }
            }

            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                val primaryAction: Pair<ImageVector, String>? = when (st.status) {
                    ComponentStatus.NotInstalled -> Icons.Rounded.CloudDownload to "$verb ${versionLabel(e.versionName)}"
                    ComponentStatus.Incomplete -> Icons.Rounded.CloudDownload to "Reinstall ${versionLabel(e.versionName)}"
                    ComponentStatus.UpdateAvailable -> Icons.Rounded.SystemUpdateAlt to "Update to ${versionLabel(e.versionName)}"
                    ComponentStatus.Installed -> null // up to date: nothing to download
                }
                if (primaryAction != null) {
                    Button(onClick = onDownload, enabled = !busy, modifier = Modifier.heightIn(min = 48.dp)) {
                        Icon(primaryAction.first, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp))
                        Text(if (working) "Working..." else primaryAction.second)
                    }
                }
                Spacer(Modifier.weight(1f))
                st.installed.forEach { c ->
                    TextButton(
                        onClick = { onDelete(c) }, enabled = !busy,
                        modifier = Modifier.heightIn(min = 48.dp).semantics { contentDescription = "Remove ${e.title} ${versionLabel(c.versionName)}" },
                        colors = ButtonDefaults.textButtonColors(contentColor = MaterialTheme.colorScheme.error)
                    ) {
                        Icon(Icons.Rounded.DeleteOutline, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(6.dp))
                        Text(if (st.installed.size > 1) "Remove ${versionLabel(c.versionName)}" else "Remove")
                    }
                }
            }
        }
    }
}

@Composable
private fun InstalledRow(c: InstalledContent, busy: Boolean, onDelete: () -> Unit) {
    val complete = remember(c.dir.path) { ContentManager.isComplete(c) }
    Row(modifier = Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(
                "${c.type} ${versionLabel(c.versionName)}" + if (!complete) " (incomplete)" else "",
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.Medium
            )
            if (c.description.isNotEmpty()) {
                Text(
                    c.description,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    maxLines = 2
                )
            }
        }
        IconButton(onClick = onDelete, enabled = !busy) {
            Icon(Icons.Rounded.DeleteOutline, contentDescription = "Remove ${c.type} ${versionLabel(c.versionName)}")
        }
    }
}
