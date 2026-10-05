package dev.zenithblue.panvklauncher

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import android.widget.Toast
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.text.selection.SelectionContainer
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.DialogProperties
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.util.Locale
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicInteger

class CloudUploadState(
    endpoint: String = PANVK_UPLOAD_ENDPOINT,
    val zipProvider: suspend (Context, File) -> File = { ctx, dir -> SessionLogs.buildCloudZip(ctx, dir) }
) {
    var endpoint by mutableStateOf(endpoint)
    var targetSession by mutableStateOf<File?>(null)
    var isUploading by mutableStateOf(false)
    var isPreparingZip by mutableStateOf(false)
    var isBusy by mutableStateOf(false)
    var showLinkDialog by mutableStateOf(false)
    var showErrorDialog by mutableStateOf(false)
    var uploadSha256 by mutableStateOf<String?>(null)
    var recordStatus by mutableStateOf<String?>(null)
    var currentZip by mutableStateOf<File?>(null)
    var pathAState by mutableStateOf(UploadPathState(name = "catbox / gofile"))
    var pathBState by mutableStateOf(UploadPathState(name = "PanVK storage (R2)"))
    var isRetryingA by mutableStateOf(false)
    var isRetryingB by mutableStateOf(false)
    val currentCancelFlag = mutableStateOf(AtomicBoolean(false))
    val uploadGeneration = AtomicInteger(0)

    fun startFlow(sessionDir: File) {
        targetSession = sessionDir
        isRetryingA = false
        isRetryingB = false
    }

    fun dismissConfirm() {
        targetSession = null
        isRetryingA = false
        isRetryingB = false
    }

    fun clearOverride(context: Context) {
        UploadPrefs.setStoredEndpoint(context, null)
        endpoint = resolveUploadEndpoint(context, null)
    }

    fun cancelUpload() {
        currentCancelFlag.value.set(true)
        uploadGeneration.incrementAndGet()
        isPreparingZip = false
        isUploading = false
        isBusy = false
        isRetryingA = false
        isRetryingB = false
    }

    fun dismissError() {
        showErrorDialog = false
        uploadGeneration.incrementAndGet()
        isBusy = false
        isRetryingA = false
        isRetryingB = false
    }

    fun dismissLink() {
        showLinkDialog = false
        uploadGeneration.incrementAndGet()
        isBusy = false
        isRetryingA = false
        isRetryingB = false
    }
}

@Composable
fun rememberCloudUploadState(
    endpoint: String? = null,
    zipProvider: suspend (Context, File) -> File = { ctx, dir -> SessionLogs.buildCloudZip(ctx, dir) }
): CloudUploadState {
    val context = LocalContext.current
    val effectiveEndpoint = remember(endpoint, context) {
        resolveUploadEndpoint(context, endpoint)
    }
    return remember(effectiveEndpoint) {
        CloudUploadState(endpoint = effectiveEndpoint, zipProvider = zipProvider)
    }
}

@Composable
fun CloudUploadFlow(state: CloudUploadState) {
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()

    DisposableEffect(Unit) {
        onDispose {
            state.cancelUpload()
        }
    }

    fun launchUpload(zip: File, localSha: String) {
        val gen = state.uploadGeneration.incrementAndGet()
        val flag = AtomicBoolean(false)
        state.currentCancelFlag.value.set(true)
        state.currentCancelFlag.value = flag
        state.isBusy = true
        state.isPreparingZip = false
        state.isUploading = true
        state.showLinkDialog = false
        state.showErrorDialog = false
        state.recordStatus = null
        state.isRetryingA = false
        state.isRetryingB = false

        coroutineScope.launch {
            if (state.uploadGeneration.get() != gen) return@launch
            if (flag.get()) {
                state.isUploading = false
                state.isBusy = false
                return@launch
            }
            val endpoint = state.endpoint
            val bInitialStatus = if (endpoint.isEmpty()) "Skipped (not configured)" else "Uploading"
            val bInitialError = if (endpoint.isEmpty()) "Skipped (not configured)" else null
            state.pathAState = UploadPathState(name = "catbox / gofile", status = "Uploading", totalBytes = zip.length())
            state.pathBState = UploadPathState(name = "PanVK storage (R2)", status = bInitialStatus, error = bInitialError, totalBytes = if (endpoint.isNotEmpty()) zip.length() else 0L)

            val appVersion = getAppVersion(context)

            try {
                coroutineScope {
                    val jobA = async(Dispatchers.IO) {
                        var cancelLoggedA = false
                        fun logCancelA() {
                            if (!cancelLoggedA) {
                                cancelLoggedA = true
                                Log.i("PanPlay", "upload cancelled")
                            }
                        }
                        if (state.uploadGeneration.get() != gen) return@async
                        if (flag.get()) {
                            logCancelA()
                            return@async
                        }
                        var lastPercentA = -1
                        try {
                            val resA = uploadToCloud(zip, appVersion, flag) { sent, total ->
                                val pct = if (total > 0) ((sent * 100) / total).toInt() else 0
                                if (pct != lastPercentA || sent == total) {
                                    lastPercentA = pct
                                    if (!flag.get() && state.uploadGeneration.get() == gen) {
                                        state.pathAState = state.pathAState.copy(bytesSent = sent, totalBytes = total)
                                    }
                                }
                            }
                            if (state.uploadGeneration.get() != gen) return@async
                            if (flag.get()) {
                                logCancelA()
                                return@async
                            }
                            if (resA.directUrl == null) {
                                if (state.uploadGeneration.get() == gen) {
                                    state.pathAState = state.pathAState.copy(
                                        url = resA.url,
                                        directUrl = null,
                                        status = "Done (not verified, gofile)",
                                        verifyStatus = "– not verified",
                                        bytesSent = zip.length(),
                                        totalBytes = zip.length()
                                    )
                                }
                            } else {
                                if (state.uploadGeneration.get() == gen) {
                                    state.pathAState = state.pathAState.copy(
                                        url = resA.url,
                                        directUrl = resA.directUrl,
                                        status = "Verifying",
                                        bytesSent = zip.length(),
                                        totalBytes = zip.length()
                                    )
                                }
                                val vA = verifyUpload(resA.directUrl, localSha, appVersion, flag)
                                if (state.uploadGeneration.get() != gen) return@async
                                if (flag.get()) {
                                    logCancelA()
                                    return@async
                                }
                                if (state.uploadGeneration.get() == gen) {
                                    if (vA == "Verified ✓") {
                                        state.pathAState = state.pathAState.copy(status = "Done ✓ verified", verifyStatus = "✓")
                                    } else {
                                        state.pathAState = state.pathAState.copy(status = "Failed: $vA", verifyStatus = "✗")
                                    }
                                }
                            }
                        } catch (e: CancellationException) {
                            logCancelA()
                            throw e
                        } catch (e: Exception) {
                            if (state.uploadGeneration.get() != gen) return@async
                            if (flag.get()) {
                                logCancelA()
                                return@async
                            }
                            val msg = friendlyUploadError(e)
                            state.pathAState = state.pathAState.copy(status = "Failed: $msg", error = msg)
                        }
                    }

                    val jobB = async(Dispatchers.IO) {
                        if (state.uploadGeneration.get() != gen) return@async
                        if (endpoint.isEmpty()) {
                            state.pathBState = state.pathBState.copy(status = "Skipped (not configured)", error = "Skipped (not configured)")
                            return@async
                        }
                        var cancelLoggedB = false
                        fun logCancelB() {
                            if (!cancelLoggedB) {
                                cancelLoggedB = true
                                Log.i("PanPlay", "upload cancelled")
                            }
                        }
                        if (state.uploadGeneration.get() != gen) return@async
                        if (flag.get()) {
                            logCancelB()
                            return@async
                        }
                        var lastPercentB = -1
                        try {
                            val resB = uploadToR2(
                                endpoint = endpoint,
                                f = zip,
                                sha256Hex = localSha,
                                app = "panplay",
                                version = appVersion,
                                cancelled = flag
                            ) { sent, total ->
                                val pct = if (total > 0) ((sent * 100) / total).toInt() else 0
                                if (pct != lastPercentB || sent == total) {
                                    lastPercentB = pct
                                    if (!flag.get() && state.uploadGeneration.get() == gen) {
                                        state.pathBState = state.pathBState.copy(bytesSent = sent, totalBytes = total)
                                    }
                                }
                            }
                            if (state.uploadGeneration.get() != gen) return@async
                            if (flag.get()) {
                                logCancelB()
                                return@async
                            }
                            if (state.uploadGeneration.get() == gen) {
                                state.pathBState = state.pathBState.copy(
                                    url = resB.url,
                                    directUrl = resB.directUrl,
                                    status = "Verifying",
                                    bytesSent = zip.length(),
                                    totalBytes = zip.length()
                                )
                            }
                            val vB = verifyUpload(resB.directUrl, localSha, appVersion, flag)
                            if (state.uploadGeneration.get() != gen) return@async
                            if (flag.get()) {
                                logCancelB()
                                return@async
                            }
                            if (state.uploadGeneration.get() == gen) {
                                if (vB == "Verified ✓") {
                                    state.pathBState = state.pathBState.copy(status = "Done ✓ verified", verifyStatus = "✓")
                                } else {
                                    state.pathBState = state.pathBState.copy(status = "Failed: $vB", verifyStatus = "✗")
                                }
                            }
                        } catch (e: CancellationException) {
                            logCancelB()
                            throw e
                        } catch (_: R2StorageNotConfiguredException) {
                            if (!flag.get() && state.uploadGeneration.get() == gen) {
                                state.pathBState = state.pathBState.copy(
                                    status = "Skipped (not configured)",
                                    error = "Skipped (not configured)",
                                    totalBytes = 0L
                                )
                            }
                        } catch (e: Exception) {
                            if (state.uploadGeneration.get() != gen) return@async
                            if (flag.get()) {
                                logCancelB()
                                return@async
                            }
                            val msg = friendlyUploadError(e)
                            state.pathBState = state.pathBState.copy(status = "Failed: $msg", error = msg)
                        }
                    }

                    jobA.await()
                    jobB.await()
                }

                if (state.uploadGeneration.get() != gen) return@launch
                if (flag.get()) {
                    state.isUploading = false
                    state.isBusy = false
                    return@launch
                }
                state.isUploading = false
                state.isBusy = false
                if (endpoint.isNotEmpty()) {
                    state.recordStatus = "Recording…"
                    val pathA = state.pathAState
                    val pathB = state.pathBState
                    val recordContext = context.applicationContext
                    CoroutineScope(Dispatchers.IO).launch {
                        val recorded = try {
                            postRecord(endpoint, buildUploadRecord(recordContext, zip, localSha, pathA, pathB))
                        } catch (_: Exception) {
                            false
                        }
                        withContext(Dispatchers.Main) {
                            if (state.uploadGeneration.get() == gen) {
                                state.recordStatus = if (recorded) "Recorded ✓" else "Record failed"
                            }
                        }
                    }
                }
                if (state.pathAState.url != null || state.pathBState.url != null) {
                    state.showLinkDialog = true
                } else {
                    state.showErrorDialog = true
                }
            } catch (_: CancellationException) {
                if (state.uploadGeneration.get() != gen) return@launch
                state.isUploading = false
                state.isBusy = false
                if (flag.get()) return@launch
                Toast.makeText(context, "Upload cancelled", Toast.LENGTH_SHORT).show()
            } catch (e: Exception) {
                if (state.uploadGeneration.get() != gen) return@launch
                state.isUploading = false
                state.isBusy = false
                if (flag.get()) return@launch
                val msg = friendlyUploadError(e)
                if (state.pathAState.url == null) state.pathAState = state.pathAState.copy(status = "Failed: $msg", error = msg)
                if (state.pathBState.url == null) state.pathBState = state.pathBState.copy(status = "Failed: $msg", error = msg)
                state.showErrorDialog = true
            }
        }
    }

    val confirmDir = state.targetSession
    if (confirmDir != null) {
        AlertDialog(
            onDismissRequest = { state.dismissConfirm() },
            title = { Text("Send to cloud?") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text("This uploads a ZIP of logs to a public file host (catbox.moe, or gofile.io as fallback) and to the PanVK project's own storage (deleted after 30 days). Upload details (device, GPU, driver and links) are also saved to the PanVK project database. Anyone with a link can download it. It may contain your device model, GPU info, Android version, game and app names, package names and file paths. It does not include accounts, contacts or personal files. Share the links only in the PanVK Telegram group. Files on catbox/gofile may not be deletable.")
                    if (state.endpoint != PANVK_UPLOAD_ENDPOINT) {
                        Text(
                            text = "Test upload endpoint override active: ${state.endpoint}",
                            color = MaterialTheme.colorScheme.error
                        )
                        TextButton(
                            onClick = { state.clearOverride(context) },
                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                            modifier = Modifier.height(28.dp)
                        ) {
                            Text("Clear override", style = MaterialTheme.typography.labelSmall)
                        }
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        val targetRun = state.targetSession
                        state.dismissConfirm()
                        if (targetRun != null) {
                            val gen = state.uploadGeneration.incrementAndGet()
                            val flag = AtomicBoolean(false)
                            state.currentCancelFlag.value.set(true)
                            state.currentCancelFlag.value = flag
                            state.isBusy = true
                            state.isPreparingZip = true
                            state.isUploading = false
                            state.showLinkDialog = false
                            state.showErrorDialog = false
                            state.isRetryingA = false
                            state.isRetryingB = false

                            coroutineScope.launch {
                                if (state.uploadGeneration.get() != gen) return@launch
                                if (flag.get()) {
                                    state.isBusy = false
                                    state.isPreparingZip = false
                                    return@launch
                                }
                                val (zip, localSha) = try {
                                    withContext(Dispatchers.IO) {
                                        val z = state.zipProvider(context, targetRun)
                                        val s = sha256(z)
                                        Pair(z, s)
                                    }
                                } catch (e: Exception) {
                                    if (state.uploadGeneration.get() != gen) return@launch
                                    state.isBusy = false
                                    state.isPreparingZip = false
                                    val msg = friendlyUploadError(e)
                                    state.pathAState = state.pathAState.copy(status = "Failed: $msg", error = msg)
                                    state.pathBState = state.pathBState.copy(status = "Failed: $msg", error = msg)
                                    state.showErrorDialog = true
                                    return@launch
                                }
                                if (state.uploadGeneration.get() != gen) return@launch
                                if (flag.get()) {
                                    state.isBusy = false
                                    state.isPreparingZip = false
                                    return@launch
                                }
                                state.currentZip = zip
                                state.uploadSha256 = localSha
                                launchUpload(zip, localSha)
                            }
                        }
                    }
                ) {
                    Text("Upload")
                }
            },
            dismissButton = {
                OutlinedButton(onClick = { state.dismissConfirm() }) {
                    Text("Cancel")
                }
            }
        )
    }

    if (state.isPreparingZip || state.isUploading) {
        AlertDialog(
            onDismissRequest = { /* non-dismissable */ },
            properties = DialogProperties(dismissOnBackPress = false, dismissOnClickOutside = false),
            title = { Text(if (state.isPreparingZip) "Preparing ZIP..." else "Uploading...") },
            text = {
                if (state.isPreparingZip) {
                    Column(
                        modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(16.dp),
                        horizontalAlignment = Alignment.CenterHorizontally
                    ) {
                        LinearProgressIndicator(modifier = Modifier.fillMaxWidth())
                        Text("Preparing ZIP...", style = MaterialTheme.typography.bodySmall)
                    }
                } else {
                    Column(
                        modifier = Modifier.fillMaxWidth().padding(top = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(16.dp)
                    ) {
                        for (path in listOf(state.pathAState, state.pathBState)) {
                            Column(
                                modifier = Modifier.fillMaxWidth(),
                                verticalArrangement = Arrangement.spacedBy(4.dp)
                            ) {
                                Text(path.name, fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.labelMedium)
                                val progress = if (path.totalBytes > 0) {
                                    (path.bytesSent.toFloat() / path.totalBytes.toFloat()).coerceIn(0f, 1f)
                                } else if (path.status.startsWith("Done") || path.status == "Verifying") {
                                    1f
                                } else {
                                    0f
                                }
                                LinearProgressIndicator(
                                    progress = { progress },
                                    modifier = Modifier.fillMaxWidth()
                                )
                                val progressText = if (path.status == "Uploading") {
                                    val percent = (progress * 100).toInt()
                                    val sentMb = path.bytesSent / (1024.0 * 1024.0)
                                    val totalMb = path.totalBytes / (1024.0 * 1024.0)
                                    String.format(Locale.US, "Uploading %.2f / %.2f MB (%d%%)", sentMb, totalMb, percent)
                                } else {
                                    path.status
                                }
                                Text(
                                    text = progressText,
                                    style = MaterialTheme.typography.bodySmall,
                                    modifier = Modifier.align(Alignment.CenterHorizontally)
                                )
                            }
                        }
                    }
                }
            },
            confirmButton = {
                OutlinedButton(
                    onClick = {
                        state.cancelUpload()
                        Toast.makeText(context, "Upload cancelled", Toast.LENGTH_SHORT).show()
                    }
                ) {
                    Text("Cancel")
                }
            }
        )
    }

    if (state.showErrorDialog) {
        val errA = state.pathAState.error ?: state.pathAState.status
        val errB = state.pathBState.error ?: state.pathBState.status
        AlertDialog(
            onDismissRequest = {
                state.dismissError()
            },
            title = { Text("Upload Failed") },
            text = {
                SelectionContainer {
                    Text("${state.pathAState.name}: $errA\n\n${state.pathBState.name}: $errB")
                }
            },
            confirmButton = {
                Button(onClick = {
                    val zip = state.currentZip
                    val sha = state.uploadSha256
                    state.dismissError()
                    if (zip != null && sha != null) {
                        launchUpload(zip, sha)
                    }
                }) {
                    Text("Retry")
                }
            },
            dismissButton = {
                OutlinedButton(onClick = {
                    state.dismissError()
                }) {
                    Text("Cancel")
                }
            }
        )
    }

    if (state.showLinkDialog) {
        val hex = state.uploadSha256 ?: ""
        val shareText = buildString {
            append("PanPlay logs:\n")
            if (state.pathAState.url != null) append(state.pathAState.url).append("\n")
            if (state.pathBState.url != null) append(state.pathBState.url).append("\n")
            append("SHA-256: $hex")
        }

        AlertDialog(
            onDismissRequest = {
                state.dismissLink()
            },
            title = { Text("Upload done") },
            text = {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(12.dp)
                ) {
                    for (p in listOf(state.pathAState, state.pathBState)) {
                        val isPathA = p.name == state.pathAState.name
                        Column(verticalArrangement = Arrangement.spacedBy(4.dp)) {
                            Text(p.name, fontWeight = FontWeight.SemiBold, style = MaterialTheme.typography.labelLarge)
                            if (p.url != null) {
                                SelectionContainer {
                                    Text(
                                        text = p.url,
                                        style = MaterialTheme.typography.bodyMedium,
                                        color = MaterialTheme.colorScheme.primary
                                    )
                                }
                                Row(
                                    verticalAlignment = Alignment.CenterVertically,
                                    horizontalArrangement = Arrangement.spacedBy(8.dp)
                                ) {
                                    val (statusText, statusTone) = when (p.verifyStatus) {
                                        "✓" -> Pair("✓", Tone.Ok)
                                        "– not verified" -> Pair("– not verified", Tone.Neutral)
                                        "✗" -> Pair("✗", Tone.Error)
                                        else -> Pair(p.verifyStatus ?: "", Tone.Neutral)
                                    }
                                    StatusPill(text = statusText, tone = statusTone)

                                    if (p.verifyStatus == "✗" && p.directUrl != null) {
                                        val isRetrying = if (isPathA) state.isRetryingA else state.isRetryingB
                                        OutlinedButton(
                                            enabled = !isRetrying,
                                            onClick = {
                                                val retryGen = state.uploadGeneration.get()
                                                val retryCancelFlag = state.currentCancelFlag.value
                                                val appVersion = getAppVersion(context)
                                                coroutineScope.launch {
                                                    if (state.uploadGeneration.get() != retryGen) return@launch
                                                    if (isPathA) state.isRetryingA = true else state.isRetryingB = true
                                                    val currentP = if (isPathA) state.pathAState else state.pathBState
                                                    if (isPathA) {
                                                        state.pathAState = state.pathAState.copy(status = "Verifying", verifyStatus = "Verifying...")
                                                    } else {
                                                        state.pathBState = state.pathBState.copy(status = "Verifying", verifyStatus = "Verifying...")
                                                    }
                                                    val newStatus = withContext(Dispatchers.IO) {
                                                        verifyUpload(currentP.directUrl, hex, appVersion, retryCancelFlag)
                                                    }
                                                    if (state.uploadGeneration.get() != retryGen) return@launch
                                                    val isOk = newStatus == "Verified ✓"
                                                    val updated = currentP.copy(
                                                        status = if (isOk) "Done ✓ verified" else "Failed: $newStatus",
                                                        verifyStatus = if (isOk) "✓" else "✗"
                                                    )
                                                    if (isPathA) state.pathAState = updated else state.pathBState = updated
                                                    if (isPathA) state.isRetryingA = false else state.isRetryingB = false
                                                }
                                            },
                                            contentPadding = PaddingValues(horizontal = 8.dp, vertical = 2.dp),
                                            modifier = Modifier.height(28.dp)
                                        ) {
                                            Text(if (isRetrying) "Retrying..." else "Retry", style = MaterialTheme.typography.labelSmall)
                                        }
                                    }
                                }
                            } else {
                                SelectionContainer {
                                    Text(
                                        text = p.error ?: p.status,
                                        style = MaterialTheme.typography.bodySmall,
                                        color = MaterialTheme.colorScheme.error
                                    )
                                }
                            }
                        }
                    }

                    SelectionContainer {
                        Text(
                            text = "SHA-256: $hex",
                            style = MaterialTheme.typography.bodySmall,
                            fontFamily = FontFamily.Monospace
                        )
                    }
                    state.recordStatus?.let {
                        Text(it, style = MaterialTheme.typography.bodySmall)
                    }
                }
            },
            confirmButton = {
                Column(
                    modifier = Modifier.fillMaxWidth(),
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        Button(
                            onClick = {
                                val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                                val clip = ClipData.newPlainText("PanPlay Upload Link", shareText)
                                clipboard.setPrimaryClip(clip)
                                Toast.makeText(context, "Link copied to clipboard", Toast.LENGTH_SHORT).show()
                            },
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Copy")
                        }
                        Button(
                            onClick = {
                                val sendIntent = Intent(Intent.ACTION_SEND).apply {
                                    type = "text/plain"
                                    putExtra(Intent.EXTRA_TEXT, shareText)
                                }
                                context.startActivity(Intent.createChooser(sendIntent, "Share Link"))
                            },
                            modifier = Modifier.weight(1f)
                        ) {
                            Text("Share")
                        }
                    }
                    Button(
                        onClick = {
                            val tgIntent = Intent(Intent.ACTION_VIEW, Uri.parse("https://t.me/+E-NhUATmkqE5ODg1"))
                            context.startActivity(tgIntent)
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Open Telegram group")
                    }
                    OutlinedButton(
                        onClick = {
                            state.dismissLink()
                        },
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Close")
                    }
                }
            }
        )
    }
}
