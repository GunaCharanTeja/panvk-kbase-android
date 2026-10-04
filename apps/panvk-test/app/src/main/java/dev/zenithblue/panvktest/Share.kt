package dev.zenithblue.panvktest

import android.app.Activity
import android.content.ClipData
import android.content.Context
import android.content.Intent
import androidx.core.content.FileProvider
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import java.util.zip.ZipEntry
import java.util.zip.ZipOutputStream

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

private fun uploadMultipart(
    urlStr: String,
    fields: Map<String, String>,
    fileFieldName: String,
    file: File,
    version: String,
    onProgress: (Float) -> Unit
): String {
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

    try {
        conn.outputStream.use { os ->
            os.write(preBytes)
            var bytesWritten = 0L
            onProgress(0f)
            file.inputStream().use { fis ->
                val buffer = ByteArray(64 * 1024)
                var read: Int
                while (fis.read(buffer).also { read = it } != -1) {
                    os.write(buffer, 0, read)
                    bytesWritten += read
                    if (fileLength > 0) {
                        onProgress((bytesWritten.toFloat() / fileLength).coerceIn(0f, 1f))
                    }
                }
            }
            os.write(postBytes)
            os.flush()
        }
        onProgress(1f)

        val code = conn.responseCode
        val stream = if (code in 200..299) conn.inputStream else conn.errorStream
        val responseBody = stream?.bufferedReader()?.use { it.readText() } ?: ""
        if (code !in 200..299) {
            throw IOException("HTTP $code: $responseBody")
        }
        return responseBody
    } finally {
        conn.disconnect()
    }
}

fun uploadToCloud(f: File, version: String, onProgress: (Float) -> Unit): String {
    val maxBytes = 200L * 1024 * 1024
    if (f.length() > maxBytes) {
        throw IllegalArgumentException("File size exceeds 200 MB limit (${f.length()} bytes)")
    }

    var catboxError: String? = null
    try {
        val catboxResponse = uploadMultipart(
            urlStr = "https://catbox.moe/user/api.php",
            fields = mapOf("reqtype" to "fileupload"),
            fileFieldName = "fileToUpload",
            file = f,
            version = version,
            onProgress = onProgress
        ).trim()
        if (catboxResponse.startsWith("https://")) {
            return catboxResponse
        }
        catboxError = "Invalid response: $catboxResponse"
    } catch (e: Exception) {
        catboxError = e.message ?: e.toString()
    }

    var gofileError: String? = null
    try {
        onProgress(0f)
        val gofileResponse = uploadMultipart(
            urlStr = "https://upload.gofile.io/uploadfile",
            fields = emptyMap(),
            fileFieldName = "file",
            file = f,
            version = version,
            onProgress = onProgress
        )
        val json = JSONObject(gofileResponse)
        val data = json.optJSONObject("data")
        val page = data?.optString("downloadPage")
        if (page != null && page.startsWith("https://")) {
            return page
        }
        gofileError = "Invalid response: $gofileResponse"
    } catch (e: Exception) {
        gofileError = e.message ?: e.toString()
    }

    throw IOException("Upload failed. Catbox: $catboxError; Gofile: $gofileError")
}
