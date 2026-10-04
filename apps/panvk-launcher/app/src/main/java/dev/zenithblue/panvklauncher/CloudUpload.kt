package dev.zenithblue.panvklauncher

import android.content.Context
import android.os.Build
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest
import java.util.Locale
import java.util.concurrent.CancellationException
import java.util.concurrent.atomic.AtomicBoolean
import java.util.zip.ZipEntry
import java.util.zip.ZipFile
import java.util.zip.ZipOutputStream

const val PANVK_UPLOAD_ENDPOINT = ""

object UploadPrefs {
    private const val PREFS_NAME = "panplay_upload"
    private const val KEY_ENDPOINT = "uploadEndpoint"

    fun getStoredEndpoint(context: Context): String? {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        return prefs.getString(KEY_ENDPOINT, null)?.takeIf { it.isNotEmpty() }
    }

    fun setStoredEndpoint(context: Context, endpoint: String?) {
        val prefs = context.getSharedPreferences(PREFS_NAME, Context.MODE_PRIVATE)
        if (endpoint.isNullOrEmpty()) {
            prefs.edit().remove(KEY_ENDPOINT).apply()
        } else {
            prefs.edit().putString(KEY_ENDPOINT, endpoint).apply()
        }
    }
}

fun isValidUploadEndpoint(endpoint: String?): Boolean {
    if (endpoint == null) return false
    val parsed = try { URL(endpoint) } catch (_: Exception) { null } ?: return false
    val protocolOk = parsed.protocol.equals("http", ignoreCase = true) || parsed.protocol.equals("https", ignoreCase = true)
    val hostOk = parsed.host.equals("127.0.0.1", ignoreCase = true) || parsed.host.equals("localhost", ignoreCase = true)
    return protocolOk && hostOk
}

fun resolveUploadEndpoint(extraEndpoint: String?): String {
    return if (isValidUploadEndpoint(extraEndpoint)) {
        extraEndpoint!!
    } else {
        PANVK_UPLOAD_ENDPOINT
    }
}

fun resolveUploadEndpoint(ctx: Context?, extraEndpoint: String? = null): String {
    if (isValidUploadEndpoint(extraEndpoint)) {
        return extraEndpoint!!
    }
    if (ctx != null) {
        val stored = UploadPrefs.getStoredEndpoint(ctx)
        if (isValidUploadEndpoint(stored)) {
            return stored!!
        }
    }
    return PANVK_UPLOAD_ENDPOINT
}

fun getAppVersion(ctx: Context): String {
    return try {
        val pInfo = ctx.packageManager.getPackageInfo(ctx.packageName, 0)
        pInfo.versionName ?: "1.1.0"
    } catch (_: Exception) {
        "1.1.0"
    }
}

fun getAppVersionCode(ctx: Context): Long {
    return try {
        val pInfo = ctx.packageManager.getPackageInfo(ctx.packageName, 0)
        if (Build.VERSION.SDK_INT >= 28) pInfo.longVersionCode else @Suppress("DEPRECATION") pInfo.versionCode.toLong()
    } catch (_: Exception) {
        6L
    }
}

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

private fun streamFileWithProgress(
    file: File,
    os: OutputStream,
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
                Thread.sleep(200)
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
    val boundary = "PanPlayBoundary" + System.currentTimeMillis()
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
        setRequestProperty("User-Agent", "PanPlay/$version")
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

fun uploadToR2(
    endpoint: String,
    f: File,
    sha256Hex: String,
    app: String = "panplay",
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
        setRequestProperty("User-Agent", "PanPlay/$version")
        setRequestProperty("Content-Type", "application/json")
        setFixedLengthStreamingMode(reqJson.size)
    }

    val resObj = postConn.cancellable(cancelled) { conn ->
        conn.outputStream.use { it.write(reqJson) }
        if (cancelled.get()) {
            throw CancellationException("Upload cancelled")
        }
        val code = conn.responseCode
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
        catboxError = e.message ?: e.toString()
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
        gofileError = e.message ?: e.toString()
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
            setRequestProperty("User-Agent", "PanPlay/$version")
        }
        conn.cancellable(cancelled) { c ->
            val code = c.responseCode
            if (cancelled.get()) throw CancellationException("Upload cancelled")
            if (code !in 200..299) {
                return@cancellable "Verify FAILED: HTTP $code"
            }
            val md = MessageDigest.getInstance("SHA-256")
            val buffer = ByteArray(64 * 1024)
            var read: Int
            c.inputStream.use { stream ->
                while (stream.read(buffer).also { read = it } != -1) {
                    if (cancelled.get()) throw CancellationException("Upload cancelled")
                    md.update(buffer, 0, read)
                }
            }
            if (cancelled.get()) throw CancellationException("Upload cancelled")
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
        "Verify FAILED: ${e.message ?: "download error"}"
    }
}
