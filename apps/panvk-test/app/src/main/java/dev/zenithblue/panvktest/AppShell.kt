package dev.zenithblue.panvktest

import android.content.res.Configuration
import androidx.compose.animation.Crossfade
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.material3.TopAppBarDefaults
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp

enum class AppTab(val title: String, val iconRes: Int) {
    Driver("Driver", R.drawable.ic_tab_driver),
    Info("Info", R.drawable.ic_tab_info),
    Tests("Tests", R.drawable.ic_tab_tests),
    Logs("Logs", R.drawable.ic_tab_logs)
}

/**
 * App chrome: top bar (logo, title, running pill), bottom navigation in portrait,
 * navigation rail in landscape.
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun AppShell(
    tab: AppTab,
    onTab: (AppTab) -> Unit,
    running: Boolean = false,
    content: @Composable (AppTab) -> Unit
) {
    val landscape = LocalConfiguration.current.orientation == Configuration.ORIENTATION_LANDSCAPE

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Row(
                        verticalAlignment = Alignment.CenterVertically,
                        horizontalArrangement = Arrangement.spacedBy(10.dp)
                    ) {
                        Image(
                            painter = painterResource(R.drawable.ic_panprobe_logo),
                            contentDescription = null,
                            modifier = Modifier.size(32.dp)
                        )
                        Text(
                            text = "PanProbe",
                            style = MaterialTheme.typography.titleMedium,
                            fontWeight = FontWeight.Bold
                        )
                    }
                },
                actions = {
                    if (running) {
                        StatusPill("Running", Tone.Accent, Modifier.padding(end = 8.dp))
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = MaterialTheme.colorScheme.surface)
            )
        },
        bottomBar = {
            if (!landscape) {
                NavigationBar {
                    AppTab.entries.forEach { t ->
                        NavigationBarItem(
                            selected = tab == t,
                            onClick = { onTab(t) },
                            icon = { Icon(painterResource(t.iconRes), contentDescription = null) },
                            label = { Text(t.title) }
                        )
                    }
                }
            }
        },
        containerColor = MaterialTheme.colorScheme.surface
    ) { inner ->
        Row(Modifier.fillMaxSize().padding(inner)) {
            if (landscape) {
                val tall = LocalConfiguration.current.screenHeightDp >= 420
                NavigationRail(containerColor = MaterialTheme.colorScheme.surfaceContainer) {
                    AppTab.entries.forEach { t ->
                        NavigationRailItem(
                            selected = tab == t,
                            onClick = { onTab(t) },
                            icon = { Icon(painterResource(t.iconRes), contentDescription = t.title) },
                            label = { Text(t.title) },
                            alwaysShowLabel = tall
                        )
                    }
                }
            }
            Crossfade(targetState = tab, modifier = Modifier.weight(1f).fillMaxSize(), label = "tab") { t ->
                Box(Modifier.fillMaxSize()) { content(t) }
            }
        }
    }
}
