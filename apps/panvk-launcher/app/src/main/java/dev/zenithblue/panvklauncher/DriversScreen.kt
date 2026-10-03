package dev.zenithblue.panvklauncher

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.DeleteOutline
import androidx.compose.material.icons.rounded.ExpandLess
import androidx.compose.material.icons.rounded.ExpandMore
import androidx.compose.material.icons.rounded.FileUpload
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material.icons.rounded.Speed
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun DriversScreen(
    drivers: List<Driver>,
    selectedDriver: Driver,
    probeResult: String?,
    isProbing: Boolean,
    importing: Boolean,
    onSelectDriver: (Driver) -> Unit,
    onImportClick: () -> Unit,
    onProbeClick: () -> Unit,
    onDeleteDriver: (Driver) -> Unit
) {
    var confirmDelete by remember { mutableStateOf<Driver?>(null) }

    PageList {
        item {
            Text(
                "The Vulkan driver is used for every game unless a game picks its own.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        item {
            FlowRow(
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                FilledTonalButton(onClick = onImportClick, enabled = !importing) {
                    Icon(Icons.Rounded.FileUpload, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text("Import driver (.adpkg.zip)")
                }
                OutlinedButton(onClick = onProbeClick, enabled = !isProbing) {
                    Icon(Icons.Rounded.Speed, contentDescription = null, modifier = Modifier.size(18.dp))
                    Spacer(Modifier.width(8.dp))
                    Text(if (isProbing) "Probing..." else "Probe active driver")
                }
            }
        }
        if (importing) item { BusyCard("Importing driver package...") }
        if (isProbing) item { BusyCard("Probing ${selectedDriver.name}...") }

        if (probeResult != null) {
            item {
                var expanded by remember { mutableStateOf(true) }
                Card(
                    modifier = Modifier.fillMaxWidth(),
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHigh)
                ) {
                    Column(Modifier.padding(16.dp)) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text("Probe result", style = MaterialTheme.typography.titleSmall, modifier = Modifier.weight(1f))
                            IconButton(onClick = { expanded = !expanded }) {
                                Icon(
                                    if (expanded) Icons.Rounded.ExpandLess else Icons.Rounded.ExpandMore,
                                    contentDescription = if (expanded) "Collapse probe result" else "Expand probe result"
                                )
                            }
                        }
                        if (expanded) {
                            SelectionContainer {
                                Text(
                                    probeResult,
                                    fontFamily = FontFamily.Monospace,
                                    fontSize = 12.sp,
                                    lineHeight = 17.sp
                                )
                            }
                        }
                    }
                }
            }
        }

        item { SectionTitle("Installed drivers") }

        if (drivers.isEmpty()) {
            item {
                EmptyState(
                    icon = Icons.Rounded.Memory,
                    title = "No drivers found",
                    body = "Import a PanVK driver package to get started.",
                    action = { Button(onClick = onImportClick) { Text("Import driver") } }
                )
            }
        }

        items(drivers, key = { it.id }) { driver ->
            DriverCard(
                driver = driver,
                selected = driver.id == selectedDriver.id,
                onSelect = { onSelectDriver(driver) },
                onDelete = { confirmDelete = driver }
            )
        }
    }

    confirmDelete?.let { d ->
        AlertDialog(
            onDismissRequest = { confirmDelete = null },
            title = { Text("Delete driver?") },
            text = { Text("${d.name} (${d.version}) will be removed from this device. Games using it fall back to the active driver.") },
            confirmButton = {
                TextButton(onClick = { confirmDelete = null; onDeleteDriver(d) }) { Text("Delete") }
            },
            dismissButton = { TextButton(onClick = { confirmDelete = null }) { Text("Cancel") } }
        )
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun DriverCard(driver: Driver, selected: Boolean, onSelect: () -> Unit, onDelete: () -> Unit) {
    var details by remember { mutableStateOf(false) }
    val cs = MaterialTheme.colorScheme
    Card(
        modifier = Modifier
            .fillMaxWidth()
            .selectable(selected = selected, onClick = onSelect, role = Role.RadioButton),
        colors = CardDefaults.cardColors(containerColor = if (selected) cs.surfaceContainerHigh else cs.surfaceContainerLow),
        border = BorderStroke(if (selected) 2.dp else 1.dp, if (selected) cs.primary else cs.outlineVariant)
    ) {
        Column(Modifier.padding(start = 8.dp, end = 12.dp, top = 8.dp, bottom = 8.dp)) {
            Row(verticalAlignment = Alignment.Top) {
                RadioButton(selected = selected, onClick = null, modifier = Modifier.padding(top = 12.dp, start = 8.dp, end = 8.dp))
                Column(Modifier.weight(1f).padding(top = 10.dp)) {
                    Text(driver.name, style = MaterialTheme.typography.titleMedium, fontWeight = FontWeight.SemiBold)
                    Text(
                        driver.version,
                        style = MaterialTheme.typography.bodyMedium,
                        color = cs.onSurfaceVariant
                    )
                    if (driver.buildId.isNotEmpty()) {
                        Text("Build ${driver.buildId}", style = MaterialTheme.typography.bodySmall, fontFamily = FontFamily.Monospace, color = cs.onSurfaceVariant)
                    }
                    Spacer(Modifier.height(8.dp))
                    FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(6.dp)) {
                        if (selected) StatusPill("Default for games", Tone.Accent)
                        StatusPill(if (driver.bundled) "Bundled with app" else "Imported", if (driver.bundled) Tone.Ok else Tone.Neutral)
                    }
                }
            }
            Spacer(Modifier.height(4.dp))
            Row(verticalAlignment = Alignment.CenterVertically) {
                TextButton(onClick = { details = !details }) {
                    Text(if (details) "Hide details" else "Details")
                    Icon(
                        if (details) Icons.Rounded.ExpandLess else Icons.Rounded.ExpandMore,
                        contentDescription = null,
                        modifier = Modifier.size(18.dp)
                    )
                }
                Spacer(Modifier.weight(1f))
                if (!driver.bundled) {
                    IconButton(onClick = onDelete) {
                        Icon(Icons.Rounded.DeleteOutline, contentDescription = "Delete driver ${driver.name}")
                    }
                }
            }
            if (details) {
                Column(
                    modifier = Modifier.padding(start = 12.dp, bottom = 8.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    if (driver.driverVersion.isNotEmpty()) LabeledValue("Mesa", driver.driverVersion)
                    if (driver.description.isNotEmpty()) LabeledValue("Description", driver.description, maxLines = 4)
                    LabeledValue("Author", driver.author)
                    if (driver.sha256.isNotEmpty()) LabeledValue("SHA-256", driver.sha256, mono = true, maxLines = 3)
                    LabeledValue("Library", driver.libPath, mono = true, maxLines = 3)
                }
            }
        }
    }
}
