package dev.zenithblue.panvklauncher

import android.content.res.Configuration
import androidx.activity.compose.BackHandler
import androidx.compose.animation.Crossfade
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ArrowBack
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.Gamepad
import androidx.compose.material.icons.rounded.Inventory2
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material.icons.rounded.Receipt
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp

enum class AppTab(val title: String, val icon: ImageVector) {
    Games("Games", Icons.Rounded.SportsEsports),
    Controls("Controls", Icons.Rounded.Gamepad),
    Components("Components", Icons.Rounded.Inventory2),
    Drivers("Drivers", Icons.Rounded.Memory),
    Settings("Settings", Icons.Rounded.Settings)
}

/**
 * App chrome: top bar (title, running pill, logs), bottom navigation in portrait, navigation rail in landscape.
 * The log view is a sub-screen of the shell (back arrow returns), not a tab.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppShell(
    tab: AppTab,
    onTab: (AppTab) -> Unit,
    showLogs: Boolean,
    onShowLogs: (Boolean) -> Unit,
    running: Boolean,
    logs: List<String>,
    onClearLogs: () -> Unit,
    onOpenSessionLogs: () -> Unit,
    /** Screen for a tab; null = launcher log. */
    content: @Composable (AppTab?) -> Unit
) {
    val landscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE
    BackHandler(enabled = showLogs) { onShowLogs(false) }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text(if (showLogs) "Launcher log" else tab.title) },
                navigationIcon = {
                    if (showLogs) {
                        IconButton(onClick = { onShowLogs(false) }) {
                            Icon(Icons.AutoMirrored.Rounded.ArrowBack, contentDescription = "Back")
                        }
                    }
                },
                actions = {
                    if (showLogs) {
                        IconButton(onClick = onOpenSessionLogs) {
                            Icon(Icons.Rounded.History, contentDescription = "Open session logs")
                        }
                        LogActions(logs, onClearLogs)
                    } else {
                        if (running) StatusPill("Running", Tone.Accent, Modifier.padding(end = 4.dp))
                        IconButton(onClick = { onShowLogs(true) }) {
                            Icon(Icons.Rounded.Receipt, contentDescription = "Open launcher log")
                        }
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surface)
            )
        },
        bottomBar = {
            if (!landscape && !showLogs) {
                NavigationBar {
                    AppTab.entries.forEach { t ->
                        NavigationBarItem(
                            selected = tab == t,
                            onClick = { onTab(t) },
                            icon = { Icon(t.icon, contentDescription = null) },
                            label = { Text(t.title) }
                        )
                    }
                }
            }
        },
        containerColor = MaterialTheme.colorScheme.surface
    ) { inner ->
        Row(Modifier.fillMaxSize().padding(inner)) {
            if (landscape && !showLogs) {
                // Short landscape screens: labels only on the selected item so four items always fit.
                val tall = LocalConfiguration.current.screenHeightDp >= 420
                NavigationRail(containerColor = MaterialTheme.colorScheme.surfaceContainer) {
                    AppTab.entries.forEach { t ->
                        NavigationRailItem(
                            selected = tab == t,
                            onClick = { onTab(t) },
                            icon = { Icon(t.icon, contentDescription = t.title) },
                            label = { Text(t.title) },
                            alwaysShowLabel = tall
                        )
                    }
                }
            }
            // Fade between tabs / log view; key = what is shown so state of each screen is kept by its own remember.
            Crossfade(targetState = if (showLogs) null else tab, modifier = Modifier.weight(1f).fillMaxSize(), label = "tab") { t ->
                Box(Modifier.fillMaxSize()) { content(t) }
            }
        }
    }
}
