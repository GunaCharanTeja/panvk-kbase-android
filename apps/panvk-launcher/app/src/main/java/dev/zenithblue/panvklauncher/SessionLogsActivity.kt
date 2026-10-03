// SPDX-License-Identifier: MIT
package dev.zenithblue.panvklauncher

import android.content.Context
import android.content.Intent
import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.content.FileProvider
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/** Shown after every game run (exit, crash, driver failure) and from Games tab "Last session logs". */
class SessionLogsActivity : ComponentActivity() {
    companion object {
        const val EXTRA_DIR = "dir"
        fun open(ctx: Context, dirName: String? = null) {
            ctx.startActivity(Intent(ctx, SessionLogsActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK).apply {
                if (dirName != null) putExtra(EXTRA_DIR, dirName)
            })
        }

        fun share(ctx: Context, dir: File) {
            val zip = SessionLogs.zip(ctx, dir)
            val uri = FileProvider.getUriForFile(ctx, ctx.packageName + ".fileprovider", zip)
            val send = Intent(Intent.ACTION_SEND).apply {
                type = "application/zip"
                putExtra(Intent.EXTRA_STREAM, uri)
                putExtra(Intent.EXTRA_SUBJECT, "PanVK session logs ${dir.name}")
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            ctx.startActivity(Intent.createChooser(send, "Share session logs").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION))
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val want = intent?.getStringExtra(EXTRA_DIR)
        setContent {
            MaterialTheme { Surface(Modifier.fillMaxSize(), color = MaterialTheme.colorScheme.background) { Screen(want) { finish() } } }
        }
    }

    @Composable
    private fun Screen(want: String?, onClose: () -> Unit) {
        val ctx = LocalContext.current
        val sessions = remember { SessionLogs.list(ctx) }
        var cur by remember { mutableStateOf(sessions.firstOrNull { it.name == want } ?: sessions.firstOrNull()) }
        var menu by remember { mutableStateOf(false) }
        Column(Modifier.fillMaxSize().systemBarsPadding().padding(12.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Text("Session logs", style = MaterialTheme.typography.titleLarge, modifier = Modifier.weight(1f))
                Button(onClick = onClose, modifier = Modifier.heightIn(min = 48.dp)) { Text("Close") }
            }
            val d = cur
            if (d == null) { Text("No sessions recorded yet."); return@Column }
            Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                Button(
                    onClick = { try { share(ctx, d) } catch (t: Throwable) { android.widget.Toast.makeText(ctx, "Share failed: $t", android.widget.Toast.LENGTH_LONG).show() } },
                    modifier = Modifier.heightIn(min = 48.dp).semantics { contentDescription = "Share as ZIP" }
                ) { Text("Share as ZIP") }
                Box {
                    OutlinedButton(onClick = { menu = true }, modifier = Modifier.heightIn(min = 48.dp)) { Text("Session: ${d.name.take(24)}") }
                    DropdownMenu(expanded = menu, onDismissRequest = { menu = false }) {
                        sessions.forEach { s -> DropdownMenuItem(text = { Text(s.name) }, onClick = { cur = s; menu = false }) }
                    }
                }
            }
            SessionView(d)
        }
    }

    @Composable
    private fun ColumnScope.SessionView(dir: File) {
        val sum = remember(dir) { SessionLogs.summary(dir) }
        val files = remember(dir) { (dir.listFiles() ?: emptyArray()).filter { it.isFile && it.name != "session.json" }.sortedBy { it.name } }
        var sel by remember(dir) { mutableStateOf(files.firstOrNull { it.name == "summary.txt" } ?: files.firstOrNull()) }
        var errOnly by remember(dir) { mutableStateOf(false) }
        val bad = sum.optBoolean("crash") || sum.optBoolean("deviceLost")
        Card(colors = CardDefaults.cardColors(containerColor = if (bad) Color(0x33E5534B) else MaterialTheme.colorScheme.surfaceVariant)) {
            Column(Modifier.fillMaxWidth().padding(10.dp)) {
                Text(sum.optString("reason", "?"), fontWeight = FontWeight.Bold, color = if (bad) Color(0xFFE5534B) else MaterialTheme.colorScheme.onSurface)
                Text(
                    "${sum.optString("game")}  |  exit ${if (sum.isNull("exit")) "n/a" else sum.optInt("exit")}  |  ${sum.optLong("durationSec")}s  |  " +
                        SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(Date(sum.optLong("time"))),
                    style = MaterialTheme.typography.bodySmall
                )
                if (!sum.isNull("tombstone")) Text("Tombstone: ${sum.optString("tombstone")}", style = MaterialTheme.typography.bodySmall)
                Text("${sum.optInt("errorCount")} error lines", style = MaterialTheme.typography.bodySmall)
                val errs = sum.optJSONArray("errors")
                Column(Modifier.heightIn(max = 110.dp).verticalScroll(rememberScrollState())) {
                    if (errs != null) for (i in 0 until minOf(errs.length(), 25))
                        Text(errs.getString(i), fontFamily = FontFamily.Monospace, fontSize = 10.sp, color = Color(0xFFE5534B), maxLines = 2)
                }
            }
        }
        Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
            FilterChip(selected = errOnly, onClick = { errOnly = !errOnly }, label = { Text("Errors only") })
            files.forEach { f -> FilterChip(selected = sel == f, onClick = { sel = f }, label = { Text(f.name) }) }
        }
        val f = sel
        if (f != null) {
            val lines by produceState<List<String>?>(null, f, errOnly) {
                value = withContext(Dispatchers.IO) {
                    val all = f.readLines()
                    if (errOnly) all.filter { SessionLogs.isError(it) } else all
                }
            }
            val l = lines
            if (l == null) Text("Loading...") else LazyColumn(Modifier.weight(1f).fillMaxWidth()) {
                items(l.size) { i ->
                    val t = l[i]
                    Text(
                        t, fontFamily = FontFamily.Monospace, fontSize = 10.sp, lineHeight = 12.sp,
                        color = when {
                            SessionLogs.isError(t) -> Color(0xFFE5534B)
                            SessionLogs.isWarn(t) -> Color(0xFFE3B341)
                            else -> MaterialTheme.colorScheme.onSurface
                        }
                    )
                }
            }
        }
    }
}
