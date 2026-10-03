package dev.zenithblue.panvklauncher

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.foundation.lazy.rememberLazyListState
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ContentCopy
import androidx.compose.material.icons.rounded.DeleteSweep
import androidx.compose.material.icons.rounded.Receipt
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

/** In-app launcher log (setup, install, run output). Copy and clear live in the top bar actions of the shell. */
@Composable
fun LogsScreen(logs: List<String>) {
    val state = rememberLazyListState()
    LaunchedEffect(logs.size) { if (logs.isNotEmpty()) state.scrollToItem(logs.size - 1) }
    if (logs.isEmpty()) {
        Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
            EmptyState(
                icon = Icons.Rounded.Receipt,
                title = "No log entries yet",
                body = "Setup, downloads and game runs write their output here."
            )
        }
        return
    }
    SelectionContainer {
        LazyColumn(
            state = state,
            modifier = Modifier.fillMaxSize(),
            contentPadding = androidx.compose.foundation.layout.PaddingValues(horizontal = 16.dp, vertical = 8.dp)
        ) {
            itemsIndexed(logs) { _, line ->
                Text(
                    text = line,
                    fontFamily = FontFamily.Monospace,
                    fontSize = 12.sp,
                    lineHeight = 17.sp,
                    color = when {
                        SessionLogs.isError(line) -> MaterialTheme.colorScheme.error
                        SessionLogs.isWarn(line) -> MaterialTheme.colorScheme.tertiary
                        else -> MaterialTheme.colorScheme.onSurface
                    },
                    modifier = Modifier
                        .fillMaxWidth()
                        .padding(vertical = 2.dp)
                )
            }
        }
    }
}

@Composable
fun LogActions(logs: List<String>, onClear: () -> Unit) {
    val ctx = LocalContext.current
    Row(horizontalArrangement = Arrangement.End) {
        IconButton(onClick = {
            val cm = ctx.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            cm.setPrimaryClip(ClipData.newPlainText("PanPlay log", logs.joinToString("\n")))
        }, enabled = logs.isNotEmpty()) { Icon(Icons.Rounded.ContentCopy, contentDescription = "Copy log") }
        IconButton(onClick = onClear, enabled = logs.isNotEmpty()) { Icon(Icons.Rounded.DeleteSweep, contentDescription = "Clear log") }
    }
}
