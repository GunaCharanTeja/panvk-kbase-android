package dev.zenithblue.panvktest

import android.app.Activity
import android.content.ClipData
import android.content.Context
import android.content.Intent
import android.util.Log
import androidx.core.content.FileProvider
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.EOFException
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.InterruptedIOException
import java.io.RandomAccessFile
import java.net.ConnectException
import java.net.HttpURLConnection
import java.net.NoRouteToHostException
import java.net.SocketException
import java.net.SocketTimeoutException
import java.net.URL
import java.net.UnknownHostException
import java.security.MessageDigest
import java.util.Locale
import java.util.concurrent.CancellationException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream
import javax.net.ssl.SSLException

fun readLogCapped(file: File, head: Int = 64 * 1024, tail: Int = 256 * 1024): String {
    require(head >= 0 && tail >= 0)
    return RandomAccessFile(file, "r").use { log ->
        val size = log.length()
        if (size <= head.toLong() + tail) {
            val bytes = ByteArray(size.toInt())
            log.readFully(bytes)
            bytes.toString(Charsets.UTF_8)
        } else {
            val headBytes = ByteArray(head)
            log.readFully(headBytes)
            val tailBytes = ByteArray(tail)
            log.seek(size - tail)
            log.readFully(tailBytes)
            val skippedMb = (size - head - tail) / (1024 * 1024)
            headBytes.toString(Charsets.UTF_8) +
                "\n...[log truncated, $skippedMb MB skipped, full log in zip]...\n" +
                tailBytes.toString(Charsets.UTF_8)
        }
    }
}

fun scanLog(file: File, onLine: (String) -> Unit) {
    file.bufferedReader().useLines { lines -> lines.forEach(onLine) }
}

fun shareFile(ctx: Context, f: File, mime: String, title: String) {
    val uri = FileProvider.getUriForFile(ctx, "${ctx.packageName}.files", f)
    val intent = Intent(Intent.ACTION_SEND).apply {
        type = mime
        putExtra(Intent.EXTRA_STREAM, uri)
        clipData = ClipData.newRawUri(null, uri)
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
    }
    val chooser = Intent.createChooser(intent, title).apply {
        addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        if (ctx !is Activity) {
            addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        }
    }
    ctx.startActivity(chooser)
}

fun zipFiles(out: File, entries: List<Pair<String, File>>) {
    out.parentFile?.mkdirs()
    ZipOutputStream(out.outputStream().buffered()).use { zos ->
        for ((entryName, file) in entries) {
            if (!file.exists()) continue
            if (file.isDirectory) {
                for (sub in file.walkTopDown()) {
                    if (sub.isFile) {
                        val rel = sub.relativeTo(file).path.replace('\\', '/')
                        val zipPath = if (entryName.isEmpty()) rel else "${entryName.trimEnd('/')}/$rel"
                        zos.putNextEntry(ZipEntry(zipPath))
                        sub.inputStream().buffered().use { it.copyTo(zos) }
                        zos.closeEntry()
                    }
                }
            } else {
                zos.putNextEntry(ZipEntry(entryName))
                file.inputStream().buffered().use { it.copyTo(zos) }
                zos.closeEntry()
            }
        }
    }
}

fun sha256(stream: InputStream): String {
    val md = MessageDigest.getInstance("SHA-256")
    val buffer = ByteArray(64 * 1024)
    var read: Int
    while (stream.read(buffer).also { read = it } != -1) {
        md.update(buffer, 0, read)
    }
    return md.digest().joinToString("") { "%02x".format(it) }
}

fun sha256(file: File): String = file.inputStream().buffered().use { sha256(it) }

// Hash the installed/extracted library (or imported override), after APK stripping.
fun driverSoSha256(path: String): String? = runCatching {
    File(path).takeIf { it.isAbsolute && it.isFile && it.canRead() }?.let { sha256(it) }
}.getOrNull()

fun gpuName(value: Any?): String? = (value as? String)?.trim()?.takeIf {
    it.isNotEmpty() && !it.equals("null", ignoreCase = true) &&
        !it.equals("unknown", ignoreCase = true) && !it.startsWith("hardware:", ignoreCase = true)
}

fun verifyZip(zip: File) {
    ZipFile(zip).use { zf ->
        val manifestEntry = zf.getEntry("manifest.json")
            ?: throw IOException("ZIP self-check failed: manifest.json")
        val manifestText = zf.getInputStream(manifestEntry).bufferedReader().use { it.readText() }
        val manifest = JSONObject(manifestText)
        val files = manifest.optJSONArray("files")
            ?: throw IOException("ZIP self-check failed: files array missing")
        if (files.length() == 0) {
            throw IOException("ZIP self-check failed: files array is empty")
        }
        val manifestPaths = mutableSetOf<String>()
        for (i in 0 until files.length()) {
            val obj = files.getJSONObject(i)
            val path = obj.getString("path")
            manifestPaths.add(path)
            val expectedSha = obj.getString("sha256")
            val entry = zf.getEntry(path) ?: throw IOException("ZIP self-check failed: $path")
            val actualSha = zf.getInputStream(entry).use { sha256(it) }
            if (!actualSha.equals(expectedSha, ignoreCase = true)) {
                throw IOException("ZIP self-check failed: $path")
            }
        }
        val entries = zf.entries()
        while (entries.hasMoreElements()) {
            val entry = entries.nextElement()
            if (entry.isDirectory || entry.name == "manifest.json") continue
            if (!manifestPaths.contains(entry.name)) {
                throw IOException("ZIP self-check failed: unlisted entry ${entry.name}")
            }
        }
    }
}

fun extractGnuBuildId(file: File): String? {
    if (!file.exists() || !file.canRead()) return null
    try {
        file.inputStream().buffered().use { fis ->
            val buf = ByteArray(64 * 1024)
            var overlap = 0
            while (true) {
                val read = fis.read(buf, overlap, buf.size - overlap)
                val total = overlap + if (read > 0) read else 0
                if (total < 16) break
                val limit = total - 16
                for (i in 0..limit) {
                    if (buf[i] == 0x04.toByte() && buf[i + 1] == 0.toByte() && buf[i + 2] == 0.toByte() && buf[i + 3] == 0.toByte() &&
                        buf[i + 8] == 0x03.toByte() && buf[i + 9] == 0.toByte() && buf[i + 10] == 0.toByte() && buf[i + 11] == 0.toByte() &&
                        buf[i + 12] == 'G'.code.toByte() && buf[i + 13] == 'N'.code.toByte() && buf[i + 14] == 'U'.code.toByte() && buf[i + 15] == 0.toByte()
                    ) {
                        val descsz = (buf[i + 4].toInt() and 0xFF) or ((buf[i + 5].toInt() and 0xFF) shl 8)
                        if (descsz in 16..64 && i + 16 + descsz <= total) {
                            return (0 until descsz).joinToString("") { "%02x".format(buf[i + 16 + it]) }
                        }
                    }
                }
                if (read <= 0) break
                val keep = total.coerceAtMost(128)
                System.arraycopy(buf, total - keep, buf, 0, keep)
                overlap = keep
            }
        }
    } catch (_: Exception) {}
    return null
}

const val PANVK_UPLOAD_ENDPOINT = "https://panvk-upload.panvk.workers.dev"

data class UploadResult(val url: String, val host: String, val directUrl: String?)

data class UploadPathState(
    val name: String,
    val bytesSent: Long = 0L,
    val totalBytes: Long = 0L,
    val status: String = "Uploading",
    val url: String? = null,
    val directUrl: String? = null,
    val verifyStatus: String? = null,
    val error: String? = null
)

private fun streamFileWithProgress(
    file: File,
    os: java.io.OutputStream,
    conn: HttpURLConnection,
    cancelled: AtomicBoolean,
    onProgress: (bytesSent: Long, total: Long) -> Unit
) {
    val fileLength = file.length()
    var bytesWritten = 0L
    onProgress(0L, fileLength)
    file.inputStream().use { fis ->
        val buffer = ByteArray(64 * 1024)
        var read: Int
        while (fis.read(buffer).also { read = it } != -1) {
            if (cancelled.get()) {
                conn.disconnect()
                throw CancellationException("Upload cancelled")
            }
            os.write(buffer, 0, read)
            bytesWritten += read
            onProgress(bytesWritten, fileLength)
        }
    }
    if (cancelled.get()) {
        conn.disconnect()
        throw CancellationException("Upload cancelled")
    }
}

fun <T> HttpURLConnection.cancellable(cancelled: AtomicBoolean, block: (HttpURLConnection) -> T): T {
    if (cancelled.get()) {
        disconnect()
        throw CancellationException("Upload cancelled")
    }
    val done = AtomicBoolean(false)
    val watcher = Thread({
        while (!done.get()) {
            if (cancelled.get()) {
                disconnect()
                break
            }
            try {
                Thread.sleep(100)
            } catch (_: InterruptedException) {
                break
            }
        }
    }, "http-cancel-watcher").apply {
        isDaemon = true
        start()
    }

    return try {
        block(this)
    } catch (e: IOException) {
        if (cancelled.get()) {
            throw CancellationException("Upload cancelled")
        }
        throw e
    } finally {
        done.set(true)
        watcher.interrupt()
        disconnect()
    }
}

private fun uploadMultipart(
    urlStr: String,
    fields: Map<String, String>,
    fileFieldName: String,
    file: File,
    version: String,
    cancelled: AtomicBoolean,
    onProgress: (bytesSent: Long, total: Long) -> Unit
): String {
    if (cancelled.get()) throw CancellationException("Upload cancelled")
    val boundary = "PanProbeBoundary" + System.currentTimeMillis()
    val lineEnd = "\r\n"
    val twoHyphens = "--"

    val preStream = ByteArrayOutputStream()
    for ((name, value) in fields) {
        preStream.write(("$twoHyphens$boundary$lineEnd").toByteArray(Charsets.UTF_8))
        preStream.write(("Content-Disposition: form-data; name=\"$name\"$lineEnd$lineEnd").toByteArray(Charsets.UTF_8))
        preStream.write(value.toByteArray(Charsets.UTF_8))
        preStream.write(lineEnd.toByteArray(Charsets.UTF_8))
    }
    preStream.write(("$twoHyphens$boundary$lineEnd").toByteArray(Charsets.UTF_8))
    preStream.write(("Content-Disposition: form-data; name=\"$fileFieldName\"; filename=\"${file.name}\"$lineEnd").toByteArray(Charsets.UTF_8))
    preStream.write(("Content-Type: application/octet-stream$lineEnd$lineEnd").toByteArray(Charsets.UTF_8))
    val preBytes = preStream.toByteArray()

    val postBytes = ("$lineEnd$twoHyphens$boundary$twoHyphens$lineEnd").toByteArray(Charsets.UTF_8)
    val fileLength = file.length()
    val totalLength = preBytes.size.toLong() + fileLength + postBytes.size.toLong()

    val url = URL(urlStr)
    val conn = (url.openConnection() as HttpURLConnection).apply {
        requestMethod = "POST"
        doOutput = true
        doInput = true
        useCaches = false
        connectTimeout = 30_000
        readTimeout = 120_000
        setRequestProperty("User-Agent", "PanProbe/$version")
        setRequestProperty("Content-Type", "multipart/form-data; boundary=$boundary")
        setFixedLengthStreamingMode(totalLength)
    }

    return conn.cancellable(cancelled) {
        it.outputStream.use { os ->
            os.write(preBytes)
            streamFileWithProgress(file, os, it, cancelled, onProgress)
            os.write(postBytes)
            os.flush()
        }
        onProgress(fileLength, fileLength)

        if (cancelled.get()) {
            throw CancellationException("Upload cancelled")
        }

        val code = it.responseCode
        val stream = if (code in 200..299) it.inputStream else it.errorStream
        val responseBody = stream?.bufferedReader()?.use { reader -> reader.readText() } ?: ""
        if (code !in 200..299) {
            throw IOException("HTTP $code: $responseBody")
        }
        responseBody
    }
}

private fun sameSchemeHostPort(u1: URL, u2: URL): Boolean {
    val port1 = if (u1.port != -1) u1.port else u1.defaultPort
    val port2 = if (u2.port != -1) u2.port else u2.defaultPort
    return u1.protocol.equals(u2.protocol, ignoreCase = true) &&
        u1.host.equals(u2.host, ignoreCase = true) &&
        port1 == port2
}

fun buildUploadRecord(
    zip: File,
    zipSha256: String,
    endpoint: String,
    pathA: UploadPathState,
    pathB: UploadPathState
): JSONObject {
    val manifest = runCatching {
        ZipFile(zip).use { zf ->
            zf.getInputStream(zf.getEntry("manifest.json")).bufferedReader().use { JSONObject(it.readText()) }
        }
    }.getOrElse { JSONObject() }
    val record = JSONObject().apply {
        put("app", "panprobe")
        put("sha256", zipSha256.lowercase(Locale.US))
        put("size", zip.length())
        put("verified_a", pathA.verifyStatus == "✓")
        put("verified_b", pathB.verifyStatus == "✓")
    }
    fun putText(key: String, value: Any?, limit: Int = 128, target: JSONObject = record) {
        if (value == null || value == JSONObject.NULL) return
        target.put(key, value.toString().filterNot { Character.isISOControl(it) }.take(limit))
    }
    val app = manifest.optJSONObject("app")
    putText("version", app?.opt("versionName"), 32)
    (app?.opt("versionCode") as? Number)?.let { record.put("version_code", it.toLong()) }
    val device = manifest.optJSONObject("device")
    putText("device_model", device?.opt("model"))
    putText("soc", device?.opt("socModel")?.takeUnless { it == JSONObject.NULL } ?: device?.opt("hardware"))
    val gpu = manifest.optJSONObject("gpu")
    putText("gpu_model", gpuName(gpu?.opt("glRenderer")) ?: gpuName(gpu?.opt("gpuModel")) ?: gpuName(gpu?.opt("deviceName")))
    putText("gpu_id", gpu?.opt("gpuId"))
    putText("arch", gpu?.opt("arch"))
    val glExtra = JSONObject()
    putText("glVendor", gpu?.opt("glVendor"), target = glExtra)
    putText("glVersion", gpu?.opt("glVersion"), target = glExtra)
    if (glExtra.length() > 0) record.put("extra_json", glExtra)
    val driver = manifest.optJSONObject("driver")
    putText("driver_name", driver?.opt("driverName"))
    putText("driver_version", driver?.opt("driverVersion"))
    putText("android_version", manifest.optJSONObject("android")?.opt("release"))
    // Use the runtime hash captured in this ZIP's manifest so both describe the same file.
    val driverSha = (driver?.opt("soSha256") as? String)?.lowercase(Locale.US)
    if (driverSha != null && driverSha.matches(Regex("[0-9a-f]{64}"))) record.put("driver_so_sha256", driverSha)
    pathA.url?.let {
        when {
            it.startsWith("https://files.catbox.moe/") -> record.put("catbox_url", it)
            it.startsWith("https://gofile.io/") -> record.put("gofile_url", it)
        }
    }
    pathB.url?.let {
        if (sameSchemeHostPort(URL(it), URL(endpoint))) record.put("r2_url", it)
    }
    // Bound UTF-8 bytes as well as character counts; preserve the hash and links.
    for (key in listOf("driver_name", "driver_version", "device_model", "soc", "gpu_model", "gpu_id", "arch", "android_version")) {
        if (record.toString().toByteArray(Charsets.UTF_8).size <= 4096) break
        record.remove(key)
    }
    return record
}

fun postRecord(endpoint: String, json: JSONObject): Boolean {
    return try {
        if (endpoint.isEmpty()) return false
        val body = json.toString().toByteArray(Charsets.UTF_8)
        if (body.size > 4096) return false
        repeat(3) { attempt ->
            if (attempt > 0) Thread.sleep(if (attempt == 1) 1_000L else 3_000L)
            var conn: HttpURLConnection? = null
            try {
                conn = (URL("${endpoint.trimEnd('/')}/record").openConnection() as HttpURLConnection).apply {
                    requestMethod = "POST"
                    doOutput = true
                    connectTimeout = 10_000
                    readTimeout = 10_000
                    instanceFollowRedirects = false
                    setRequestProperty("Content-Type", "application/json")
                    setFixedLengthStreamingMode(body.size)
                }
                conn.outputStream.use { it.write(body) }
                val code = conn.responseCode
                if (code == 204 || code == 200) return true
            } catch (_: Exception) {
                // Recording is best effort and must not affect upload results.
            } finally {
                conn?.disconnect()
            }
        }
        false
    } catch (_: Exception) {
        false
    }
}

class R2StorageNotConfiguredException : IOException("Skipped (not configured)")
class ProjectStorageTooBigException : IOException("Too big for project storage")

fun uploadToR2(
    endpoint: String,
    f: File,
    sha256Hex: String,
    app: String = "panprobe",
    version: String,
    cancelled: AtomicBoolean = AtomicBoolean(false),
    onProgress: (bytesSent: Long, total: Long) -> Unit
): UploadResult {
    if (cancelled.get()) throw CancellationException("Upload cancelled")
    val reqJson = JSONObject().apply {
        put("app", app)
        put("version", version)
        put("size", f.length())
        put("sha256", sha256Hex)
    }.toString().toByteArray(Charsets.UTF_8)

    val postUrl = URL("${endpoint.trimEnd('/')}/upload-url")
    val postConn = (postUrl.openConnection() as HttpURLConnection).apply {
        requestMethod = "POST"
        doOutput = true
        doInput = true
        useCaches = false
        connectTimeout = 30_000
        readTimeout = 120_000
        setRequestProperty("User-Agent", "PanProbe/$version")
        setRequestProperty("Content-Type", "application/json")
        setFixedLengthStreamingMode(reqJson.size)
    }

    val resObj = postConn.cancellable(cancelled) { conn ->
        conn.outputStream.use { it.write(reqJson) }
        if (cancelled.get()) {
            throw CancellationException("Upload cancelled")
        }
        val code = conn.responseCode
        if (code == 503) throw R2StorageNotConfiguredException()
        if (code == 413) throw ProjectStorageTooBigException()
        val stream = if (code in 200..299) conn.inputStream else conn.errorStream
        val responseBody = stream?.bufferedReader()?.use { it.readText() } ?: ""
        if (code !in 200..299) {
            throw IOException(if (code == 429) "rate limited" else responseBody.ifEmpty { "HTTP $code" })
        }
        JSONObject(responseBody)
    }
    val uploadUrl = resObj.getString("uploadUrl")
    val method = resObj.optString("method", "PUT").ifEmpty { "PUT" }
    val headersObj = resObj.optJSONObject("headers")
    val downloadUrl = resObj.getString("downloadUrl")

    val parsedEndpoint = try { URL(endpoint) } catch (_: Exception) { null }
    val parsedUpload = try { URL(uploadUrl) } catch (_: Exception) { null }
    val parsedDownload = try { URL(downloadUrl) } catch (_: Exception) { null }

    val uploadOk = parsedUpload != null && (
        (parsedUpload.protocol.equals("https", ignoreCase = true) &&
            parsedUpload.host.lowercase(Locale.US).endsWith(".r2.cloudflarestorage.com")) ||
        (parsedEndpoint != null && sameSchemeHostPort(parsedUpload, parsedEndpoint))
    )
    val downloadOk = parsedDownload != null && parsedEndpoint != null &&
        sameSchemeHostPort(parsedDownload, parsedEndpoint)

    if (!uploadOk || !downloadOk) {
        throw IOException("R2: unexpected upload URL host")
    }

    if (cancelled.get()) throw CancellationException("Upload cancelled")

    val putUrl = URL(uploadUrl)
    val putConn = (putUrl.openConnection() as HttpURLConnection).apply {
        requestMethod = method
        doOutput = true
        doInput = true
        useCaches = false
        connectTimeout = 30_000
        readTimeout = 120_000
        setFixedLengthStreamingMode(f.length())
        if (headersObj != null) {
            for (key in headersObj.keys()) {
                // Content-Length comes from setFixedLengthStreamingMode (same value the Worker signed).
                if (key.equals("content-length", ignoreCase = true)) {
                    if (headersObj.getString(key) != f.length().toString()) throw IOException("size mismatch")
                    continue
                }
                setRequestProperty(key, headersObj.getString(key))
            }
        }
    }

    putConn.cancellable(cancelled) { conn ->
        conn.outputStream.use { os ->
            streamFileWithProgress(f, os, conn, cancelled, onProgress)
            os.flush()
        }
        onProgress(f.length(), f.length())

        if (cancelled.get()) {
            throw CancellationException("Upload cancelled")
        }

        val putCode = conn.responseCode
        val putStream = if (putCode in 200..299) conn.inputStream else conn.errorStream
        val putBody = putStream?.bufferedReader()?.use { it.readText() } ?: ""
        if (putCode !in 200..299) {
            throw IOException(if (putCode == 429) "rate limited" else putBody.ifEmpty { "HTTP $putCode" })
        }
    }
    return UploadResult(url = downloadUrl, host = "r2", directUrl = downloadUrl)
}

fun uploadToCloud(
    f: File,
    version: String,
    cancelled: AtomicBoolean = AtomicBoolean(false),
    onProgress: (bytesSent: Long, total: Long) -> Unit
): UploadResult {
    val maxBytes = 200L * 1024 * 1024
    if (f.length() > maxBytes) {
        throw IllegalArgumentException("File size exceeds 200 MB limit (${f.length()} bytes)")
    }

    var catboxError: String? = null
    try {
        if (cancelled.get()) throw CancellationException("Upload cancelled")
        val catboxResponse = uploadMultipart(
            urlStr = "https://catbox.moe/user/api.php",
            fields = mapOf("reqtype" to "fileupload"),
            fileFieldName = "fileToUpload",
            file = f,
            version = version,
            cancelled = cancelled,
            onProgress = onProgress
        ).trim()
        if (catboxResponse.startsWith("https://")) {
            return UploadResult(url = catboxResponse, host = "catbox", directUrl = catboxResponse)
        }
        catboxError = "Invalid response: $catboxResponse"
    } catch (e: CancellationException) {
        throw e
    } catch (e: Exception) {
        if (cancelled.get()) throw CancellationException("Upload cancelled")
        catboxError = friendlyUploadError(e)
    }

    if (cancelled.get()) throw CancellationException("Upload cancelled")

    var gofileError: String? = null
    try {
        onProgress(0L, f.length())
        val gofileResponse = uploadMultipart(
            urlStr = "https://upload.gofile.io/uploadfile",
            fields = emptyMap(),
            fileFieldName = "file",
            file = f,
            version = version,
            cancelled = cancelled,
            onProgress = onProgress
        )
        val json = JSONObject(gofileResponse)
        val data = json.optJSONObject("data")
        val page = data?.optString("downloadPage")
        if (page != null && page.startsWith("https://")) {
            return UploadResult(url = page, host = "gofile", directUrl = null)
        }
        gofileError = "Invalid response: $gofileResponse"
    } catch (e: CancellationException) {
        throw e
    } catch (e: Exception) {
        if (cancelled.get()) throw CancellationException("Upload cancelled")
        gofileError = friendlyUploadError(e)
    }

    throw IOException("Upload failed. Catbox: $catboxError; Gofile: $gofileError")
}

fun verifyUpload(
    directUrl: String?,
    expectedSha256: String,
    version: String,
    cancelled: AtomicBoolean = AtomicBoolean(false)
): String {
    if (directUrl == null) return "Not verified (gofile has no direct link)"
    if (cancelled.get()) throw CancellationException("Upload cancelled")
    return try {
        val conn = (URL(directUrl).openConnection() as HttpURLConnection).apply {
            requestMethod = "GET"
            connectTimeout = 30_000
            readTimeout = 120_000
            setRequestProperty("User-Agent", "PanProbe/$version")
        }
        conn.cancellable(cancelled) { c ->
            val code = c.responseCode
            if (cancelled.get()) {
                c.disconnect()
                throw CancellationException("Upload cancelled")
            }
            if (code !in 200..299) {
                return@cancellable "Verify FAILED: HTTP $code"
            }
            val md = MessageDigest.getInstance("SHA-256")
            val buffer = ByteArray(64 * 1024)
            var read: Int
            c.inputStream.use { stream ->
                while (stream.read(buffer).also { read = it } != -1) {
                    if (cancelled.get()) {
                        c.disconnect()
                        throw CancellationException("Upload cancelled")
                    }
                    md.update(buffer, 0, read)
                }
            }
            if (cancelled.get()) {
                c.disconnect()
                throw CancellationException("Upload cancelled")
            }
            val downloadSha = md.digest().joinToString("") { "%02x".format(it) }
            if (downloadSha.equals(expectedSha256, ignoreCase = true)) {
                "Verified ✓"
            } else {
                "Verify FAILED: hash mismatch"
            }
        }
    } catch (e: CancellationException) {
        throw e
    } catch (e: Exception) {
        if (cancelled.get()) throw CancellationException("Upload cancelled")
        "Verify FAILED: ${friendlyUploadError(e)}"
    }
}

private val INTERNAL_OBJECT_REGEX = Regex("""[A-Za-z_][\w.]*@[0-9a-fA-F]+""")

private fun isInternalMessage(msg: String): Boolean {
    return INTERNAL_OBJECT_REGEX.containsMatchIn(msg) ||
        msg.contains("com.android.", ignoreCase = true) ||
        msg.contains("java.", ignoreCase = true) ||
        msg.contains("okhttp", ignoreCase = true)
}

fun friendlyUploadError(e: Throwable): String {
    Log.w("PanVKUpload", e.message ?: "Upload error", e)
    return when {
        e is SocketTimeoutException || (e is InterruptedIOException && e.message?.contains("timeout", ignoreCase = true) == true) ->
            "Timed out"
        e is UnknownHostException || e is ConnectException || e is NoRouteToHostException ->
            "Host not reachable"
        e is SSLException ->
            "Secure connection failed"
        e is IOException -> {
            val msg = e.message
            if (e is EOFException || e is SocketException || msg?.contains("unexpected end of stream", ignoreCase = true) == true) {
                "Network error: connection lost"
            } else if (msg.isNullOrBlank() || isInternalMessage(msg)) {
                "Network error: connection lost"
            } else {
                msg
            }
        }
        else -> {
            val msg = e.message
            if (!msg.isNullOrBlank() && !isInternalMessage(msg)) {
                msg
            } else {
                "Upload failed"
            }
        }
    }
}
