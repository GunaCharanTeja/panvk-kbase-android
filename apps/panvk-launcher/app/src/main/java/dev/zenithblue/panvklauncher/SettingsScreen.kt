package dev.zenithblue.panvklauncher

import android.os.Build
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.selection.toggleable
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.FolderOpen
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.Monitor
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Stop
import androidx.compose.material.icons.rounded.Terminal
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.SegmentedButton
import androidx.compose.material3.SegmentedButtonDefaults
import androidx.compose.material3.SingleChoiceSegmentedButtonRow
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import java.io.File

/** App + Wine settings. Replaces the old "Wine" tab: every action it had is here. */
@OptIn(ExperimentalMaterial3Api::class, ExperimentalLayoutApi::class)
@Composable
fun SettingsScreen(
    themeMode: String,
    onThemeMode: (String) -> Unit,
    dynamicColor: Boolean,
    onDynamicColor: (Boolean) -> Unit,
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
    onSetRes: (String) -> Unit,
    displayShm: Boolean,
    onToggleShm: (Boolean) -> Unit,
    onOpenDisplay: () -> Unit,
    onOpenScreen: () -> Unit,
    onLaunchExplorer: () -> Unit,
    onRunCmdVer: () -> Unit,
    onPickExe: () -> Unit,
    onRunManualExe: (String) -> Unit,
    onRunRecentExe: (String) -> Unit,
    onStop: () -> Unit,
    onOpenAppLog: () -> Unit,
    onOpenSessionLogs: () -> Unit,
    onOpenControls: () -> Unit = {}
) {
    var manualPath by remember { mutableStateOf("") }
    val actionEnabled = !isRunning && !isSettingUp && isSetup
    fun ver(type: String) = installedComponents.firstOrNull { it.type == type && ContentManager.isComplete(it) }?.versionName
        ?.let { versionLabel(it) } ?: "not installed"

    PageList {
        item { SectionTitle("Wine environment") }
        item {
            SettingsCard {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(
                            when {
                                isSettingUp -> "Setting up container..."
                                isSetup -> "Container ready"
                                else -> "Container not set up"
                            },
                            style = MaterialTheme.typography.titleMedium
                        )
                        if (!isSetup && !isSettingUp) {
                            Text(
                                "Install a rootfs and Wine in Components, setup then runs automatically.",
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant
                            )
                        }
                    }
                    when {
                        isRunning -> StatusPill("Running", Tone.Accent)
                        isSetup -> StatusPill("Ready", Tone.Ok)
                        isSettingUp -> StatusPill("Setting up", Tone.Warn)
                        else -> StatusPill("Not set up", Tone.Error)
                    }
                }
                if (isSettingUp) androidx.compose.material3.LinearProgressIndicator(Modifier.fillMaxWidth())
                Column {
                    InfoRow("Rootfs", ver("imagefs"))
                    InfoRow("Wine", ver("Proton"))
                    InfoRow("FEX", ver("FEXCore"))
                    InfoRow("DXVK", ver("DXVK"))
                    InfoRow("PanVK driver", selectedDriver.label)
                }
                if (isRunning) {
                    Button(
                        onClick = onStop,
                        colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.error, contentColor = MaterialTheme.colorScheme.onError),
                        modifier = Modifier.heightIn(min = 48.dp)
                    ) {
                        Icon(Icons.Rounded.Stop, contentDescription = null)
                        Spacer(Modifier.width(8.dp))
                        Text("Stop Wine")
                    }
                }
            }
        }

        item { SectionTitle("Display") }
        item {
            SettingsCard {
                Text(displayStatus, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                SwitchRow("Built-in X server", "Off uses Termux:X11", builtinDisplay, onToggleBuiltin, enabled = !isRunning)
                if (builtinDisplay) {
                    SwitchRow("MIT-SHM present", "Shared-memory frame transfer", displayShm, onToggleShm, enabled = !isRunning)
                    ResolutionField("Default resolution", displayRes, onSetRes, enabled = !isRunning)
                }
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilledTonalButton(onClick = onOpenDisplay, modifier = Modifier.heightIn(min = 48.dp)) {
                        Icon(Icons.Rounded.Monitor, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp)); Text("Open Termux:X11")
                    }
                    OutlinedButton(onClick = onOpenScreen, modifier = Modifier.heightIn(min = 48.dp)) { Text("Framebuffer view") }
                }
            }
        }

        item { SectionTitle("Controller overlay") }
        item {
            val ctx = androidx.compose.ui.platform.LocalContext.current
            var opacity by remember { mutableStateOf(OverlayPrefs.opacity(ctx)) }
            var size by remember { mutableStateOf(OverlayPrefs.size(ctx)) }
            var haptics by remember { mutableStateOf(OverlayPrefs.haptics(ctx)) }
            SettingsCard {
                Column {
                    Row { Text("Opacity", Modifier.weight(1f)); Text("${(opacity * 100).toInt()}%", style = MaterialTheme.typography.labelLarge) }
                    androidx.compose.material3.Slider(
                        opacity, { opacity = it }, valueRange = 0.2f..1f,
                        onValueChangeFinished = { OverlayPrefs.setOpacity(ctx, opacity) },
                        modifier = Modifier.semantics { contentDescription = "Overlay opacity ${(opacity * 100).toInt()} percent" }
                    )
                }
                Column {
                    Row { Text("Button size", Modifier.weight(1f)); Text("${(size * 100).toInt()}%", style = MaterialTheme.typography.labelLarge) }
                    androidx.compose.material3.Slider(
                        size, { size = it }, valueRange = 0.7f..1.4f,
                        onValueChangeFinished = { OverlayPrefs.setSize(ctx, size) },
                        modifier = Modifier.semantics { contentDescription = "Overlay button size ${(size * 100).toInt()} percent" }
                    )
                    Text(
                        "Buttons never go below 40dp and never overlap; on small screens the largest size that fits is used.",
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant
                    )
                }
                Text("Haptic feedback", style = MaterialTheme.typography.bodyLarge)
                SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                    val opts = listOf("off" to "Off", "light" to "Light", "strong" to "Strong")
                    opts.forEachIndexed { i, (v, label) ->
                        SegmentedButton(
                            selected = haptics == v,
                            onClick = { haptics = v; OverlayPrefs.setHaptics(ctx, v) },
                            shape = SegmentedButtonDefaults.itemShape(i, opts.size),
                            modifier = Modifier.heightIn(min = 48.dp)
                        ) { Text(label) }
                    }
                }
                OutlinedButton(onClick = onOpenControls, modifier = Modifier.heightIn(min = 48.dp)) { Text("Controller configs") }
            }
        }

        item { SectionTitle("Graphics") }
        item {
            SettingsCard {
                SwitchRow("DXVK", "Direct3D 8/9/10/11 to Vulkan", isDxvkEnabled, onToggleDxvk)
                Text(
                    "Enabling DXVK is not a game compatibility result. ARM64EC smoke tests do not validate x86/i686 WOW64 games; 32-bit staging can fail on Mali kbase SAME_VA.",
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
        }

        item { SectionTitle("Wine tools") }
        item {
            SettingsCard {
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    FilledTonalButton(onClick = onLaunchExplorer, enabled = actionEnabled, modifier = Modifier.heightIn(min = 48.dp)) {
                        Icon(Icons.Rounded.FolderOpen, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp)); Text("Explorer")
                    }
                    FilledTonalButton(onClick = onRunCmdVer, enabled = actionEnabled, modifier = Modifier.heightIn(min = 48.dp)) {
                        Icon(Icons.Rounded.Terminal, contentDescription = null, modifier = Modifier.size(18.dp))
                        Spacer(Modifier.width(8.dp)); Text("wine cmd /c ver")
                    }
                    FilledTonalButton(onClick = onPickExe, enabled = actionEnabled, modifier = Modifier.heightIn(min = 48.dp)) {
                        Text("Pick .exe")
                    }
                }
                OutlinedTextField(
                    value = manualPath,
                    onValueChange = { manualPath = it },
                    label = { Text("Run executable by path") },
                    placeholder = { Text("/storage/emulated/0/Download/app.exe") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )
                Button(
                    onClick = { onRunManualExe(manualPath.trim()) },
                    enabled = actionEnabled && manualPath.isNotBlank(),
                    modifier = Modifier.heightIn(min = 48.dp)
                ) {
                    Icon(Icons.Rounded.PlayArrow, contentDescription = null)
                    Spacer(Modifier.width(8.dp)); Text("Run")
                }
            }
        }

        item { SectionTitle("Recent executables") }
        if (recentExes.isEmpty()) {
            item {
                EmptyState(Icons.Rounded.History, "No recent executables", "Executables you run by path or pick will show up here.")
            }
        } else {
            items(recentExes) { exePath ->
                Card(
                    modifier = Modifier
                        .fillMaxWidth()
                        .clickable(enabled = actionEnabled, onClickLabel = "Run ${File(exePath).name}") { onRunRecentExe(exePath) },
                    colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)
                ) {
                    Row(Modifier.padding(horizontal = 16.dp, vertical = 8.dp).heightIn(min = 48.dp), verticalAlignment = Alignment.CenterVertically) {
                        Column(Modifier.weight(1f)) {
                            Text(File(exePath).name, style = MaterialTheme.typography.bodyLarge, fontWeight = FontWeight.Medium)
                            Text(
                                exePath, style = MaterialTheme.typography.labelSmall, fontFamily = FontFamily.Monospace,
                                color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis
                            )
                        }
                        Icon(Icons.Rounded.PlayArrow, contentDescription = null, tint = MaterialTheme.colorScheme.primary)
                    }
                }
            }
        }

        item { SectionTitle("Logs") }
        item {
            SettingsCard {
                FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = onOpenAppLog, modifier = Modifier.heightIn(min = 48.dp)) { Text("Launcher log") }
                    OutlinedButton(onClick = onOpenSessionLogs, modifier = Modifier.heightIn(min = 48.dp)) { Text("Last session logs") }
                }
            }
        }

        item { SectionTitle("Appearance") }
        item {
            SettingsCard {
                Text("Theme", style = MaterialTheme.typography.bodyLarge)
                SingleChoiceSegmentedButtonRow(Modifier.fillMaxWidth()) {
                    val opts = listOf("system" to "System", "light" to "Light", "dark" to "Dark")
                    opts.forEachIndexed { i, (v, label) ->
                        SegmentedButton(
                            selected = themeMode == v,
                            onClick = { onThemeMode(v) },
                            shape = SegmentedButtonDefaults.itemShape(i, opts.size),
                            modifier = Modifier.heightIn(min = 48.dp)
                        ) { Text(label) }
                    }
                }
                if (Build.VERSION.SDK_INT >= 31) {
                    SwitchRow("Dynamic color (Material You)", "Use the wallpaper's colors", dynamicColor, onDynamicColor)
                }
            }
        }
    }
}

@Composable
private fun SettingsCard(content: @Composable androidx.compose.foundation.layout.ColumnScope.() -> Unit) {
    Card(
        modifier = Modifier.fillMaxWidth(),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)
    ) {
        Column(Modifier.padding(16.dp), verticalArrangement = Arrangement.spacedBy(12.dp), content = content)
    }
}

@Composable
private fun SwitchRow(title: String, subtitle: String, checked: Boolean, onChange: (Boolean) -> Unit, enabled: Boolean = true) {
    Row(
        modifier = Modifier
            .fillMaxWidth()
            .heightIn(min = 56.dp)
            .toggleable(value = checked, enabled = enabled, role = Role.Switch, onValueChange = onChange),
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Switch(checked = checked, onCheckedChange = null, enabled = enabled)
    }
}
