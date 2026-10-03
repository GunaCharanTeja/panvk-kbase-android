package dev.zenithblue.panvklauncher

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Lock
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.ModalBottomSheet
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.material3.rememberModalBottomSheetState
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp

/**
 * "Launch" card shown before a game starts. Lists every runtime part with its real installed version
 * (read-only) and lets the user change only resolution and driver. Both choices are saved on the game.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LaunchSheet(
    game: Shortcut,
    drivers: List<Driver>,
    activeDriver: Driver,
    defaultResolution: String,
    busy: Boolean,
    onDismiss: () -> Unit,
    onLaunch: (Shortcut) -> Unit
) {
    val ctx = LocalContext.current
    var ctlRev by remember { mutableStateOf(0) }
    var editCtl by remember { mutableStateOf(false) }
    val info = remember(game.id, game.exe, game.arch, ctlRev) { LaunchInfoResolver.resolve(ctx, game) }
    if (editCtl) GameControllerEditor(game.id, game.name, info.exePath) { editCtl = false; ctlRev++ }
    var resolution by remember(game.id) { mutableStateOf(game.resolution) }
    // Driver ids can be legacy ("bundled") or point at a deleted driver: show those as "default".
    var driverId by remember(game.id) {
        mutableStateOf(DriverManager.find(ctx, drivers, game.driver)?.id ?: "")
    }
    val chosenDriver = if (driverId.isEmpty()) activeDriver else drivers.firstOrNull { it.id == driverId } ?: activeDriver

    val problems = buildList {
        info.missing.forEach { add("${it.label} is not installed. Install it in Components.") }
        if (!info.exeExists) add("Executable not found on this device.")
        if (busy) add("Wine is already running. Stop it first.")
    }

    // Default M3 sheet caps at 640dp; landscape gets a wider two-column sheet so Launch stays on screen.
    ModalBottomSheet(
        onDismissRequest = onDismiss,
        sheetState = rememberModalBottomSheetState(skipPartiallyExpanded = true),
        sheetMaxWidth = 960.dp
    ) {
        BoxWithConstraints(Modifier.fillMaxWidth()) {
            val wide = maxWidth >= 640.dp
            val header: @Composable () -> Unit = {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    GameIcon(game, size = 56)
                    Spacer(Modifier.width(14.dp))
                    Column(Modifier.weight(1f)) {
                        Text("Launch", style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary)
                        Text(game.name, style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.SemiBold, maxLines = 2, overflow = TextOverflow.Ellipsis)
                    }
                }
            }
            val runtime: @Composable () -> Unit = {
                Card(colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerLow)) {
                    Column(Modifier.padding(horizontal = 16.dp, vertical = 8.dp)) {
                        Row(Modifier.padding(vertical = 4.dp), verticalAlignment = Alignment.CenterVertically) {
                            Icon(Icons.Rounded.Lock, contentDescription = null, modifier = Modifier.size(14.dp), tint = MaterialTheme.colorScheme.onSurfaceVariant)
                            Spacer(Modifier.width(6.dp))
                            Text("Runtime (read-only)", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                        }
                        val dxvk = info.dxvk.copy(version = info.dxvk.version + if (info.dxvkEnabled) " · on" else " · off")
                        listOf(info.rootfs, info.wine, info.fex, dxvk, info.controller).forEach { r ->
                            InfoRow(r.label, r.name, r.version, subIsError = r.missing)
                        }
                        InfoRow("PanVK driver", chosenDriver.label, chosenDriver.buildId.ifEmpty { chosenDriver.driverVersion })
                        InfoRow("Executable", info.exePath, info.arch)
                        // Controller assignment is set in the game's Edit sheet; its mapping can be edited from here.
                        TextButton(onClick = { editCtl = true }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Edit controls…") }
                    }
                }
            }
            val choices: @Composable () -> Unit = {
                Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                    Text("Change for this game", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                    ResolutionField("Resolution", resolution, { resolution = it }, defaultLabel = "Default ($defaultResolution)")
                    DropdownField(
                        label = "PanVK driver",
                        options = listOf("" to "Default (${activeDriver.label})") + drivers.map { it.id to it.label },
                        selected = driverId,
                        onSelect = { driverId = it }
                    )
                    if (problems.isNotEmpty()) {
                        Column(
                            modifier = Modifier.semantics(mergeDescendants = true) {},
                            verticalArrangement = Arrangement.spacedBy(2.dp)
                        ) {
                            problems.forEach { Text(it, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.error) }
                        }
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(12.dp), verticalAlignment = Alignment.CenterVertically) {
                        TextButton(onClick = onDismiss, modifier = Modifier.heightIn(min = 52.dp)) { Text("Cancel") }
                        Button(
                            onClick = { onLaunch(game.copy(resolution = resolution, driver = driverId)) },
                            enabled = problems.isEmpty(),
                            modifier = Modifier.weight(1f).heightIn(min = 52.dp)
                        ) {
                            Icon(Icons.Rounded.PlayArrow, contentDescription = null)
                            Spacer(Modifier.width(8.dp))
                            Text("Launch", style = MaterialTheme.typography.titleMedium)
                        }
                    }
                }
            }
            Column(
                modifier = Modifier
                    .fillMaxWidth()
                    .verticalScroll(rememberScrollState())
                    .padding(horizontal = 20.dp)
                    .navigationBarsPadding(),
                verticalArrangement = Arrangement.spacedBy(14.dp)
            ) {
                if (wide) {
                    header()
                    Row(horizontalArrangement = Arrangement.spacedBy(20.dp)) {
                        Column(Modifier.weight(1.15f)) { runtime() }
                        Column(Modifier.weight(1f)) { choices() }
                    }
                } else {
                    header(); runtime(); choices()
                }
                Spacer(Modifier.height(16.dp))
            }
        }
    }
}
