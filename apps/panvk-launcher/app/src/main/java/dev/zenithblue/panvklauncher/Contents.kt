package dev.zenithblue.panvklauncher

import android.content.Context
import android.net.Uri
import com.github.luben.zstd.ZstdInputStream
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.apache.commons.compress.archivers.tar.TarArchiveInputStream
import org.json.JSONObject
import org.tukaani.xz.XZInputStream
import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL
import java.nio.file.Files
import java.nio.file.LinkOption
import java.security.MessageDigest
import java.util.Comparator

data class InstalledContent(
    val type: String,
    val versionName: String,
    val description: String,
    val dir: File
)

data class CatalogEntry(
    val type: String,
    val name: String,
    val url: String,
    val sha256: String?
)

object ContentManager {

    val CATALOG = listOf(
        CatalogEntry(
            type = "Proton",
            name = "proton-11.0-2-arm64ec (GameNative bionic)",
            url = "https://github.com/GameNative/proton-wine/releases/download/proton-11.0-2-20260928/proton-11.0-2-arm64ec.wcp",
            sha256 = "fffa467241bdae3eacd6ceb7e8096bb7793d617ce53a198dae8bc63a3453f595"
        ),
        CatalogEntry(
            type = "FEXCore",
            name = "FEXCore-2609 (WCP Hub)",
            url = "https://github.com/Arihany/WinlatorWCPHub/releases/download/FEXCore/FEXCore-2609.wcp",
            sha256 = "520c31b8ea601baf691da4577f53034e80f4b13bcbfe9d96167f2c402c1db9d1"
        ),
        CatalogEntry(
            type = "DXVK",
            name = "dxvk-arm64ec-3.1.1 (WCP Hub)",
            url = "https://github.com/Arihany/WinlatorWCPHub/releases/download/DXVK-ARM64EC/dxvk-arm64ec-3.1.1.wcp",
            sha256 = "f3765e3589a5b84888d52cc03f26e93c2d3db3565def47dc63cb75a183aaaaaa"
        ),
        CatalogEntry(
            type = "imagefs",
            name = "imagefs_bionic rootfs (GameNative-hosted, licence unclear)",
            url = "https://downloads.gamenative.app/imagefs_bionic.txz",
            sha256 = "368db62bfc58b72c97e5169bda9aa64d4246f07964c447e27c79a065e7e9c48b"
        )
    )

    fun list(context: Context): List<InstalledContent> {
        val list = mutableListOf<InstalledContent>()
        val contentsDir = File(context.filesDir, "contents")
        if (contentsDir.exists() && contentsDir.isDirectory) {
            val typeDirs = contentsDir.listFiles()?.filter { it.isDirectory } ?: emptyList()
            for (typeDir in typeDirs) {
                val verDirs = typeDir.listFiles()?.filter { it.isDirectory } ?: emptyList()
                for (verDir in verDirs) {
                    val profileFile = File(verDir, "profile.json")
                    if (!profileFile.isFile || profileFile.length() > 64 * 1024L) continue
                    try {
                        val json = JSONObject(profileFile.readText())
                        val type = json.optString("type", typeDir.name)
                        val versionName = json.optString("versionName", verDir.name)
                        val description = json.optString("description", "")
                        list.add(
                            InstalledContent(
                                type = type,
                                versionName = versionName,
                                description = description,
                                dir = verDir
                            )
                        )
                    } catch (_: Exception) {
                        // Skip unreadable or corrupted profile.json
                    }
                }
            }
        }
        return list
    }

    fun deleteTree(f: File) {
        val p = f.toPath()
        if (!Files.exists(p, LinkOption.NOFOLLOW_LINKS)) return
        Files.walk(p).use {
            it.sorted(Comparator.reverseOrder()).forEach { x ->
                Files.deleteIfExists(x)
            }
        }
    }

    fun delete(context: Context, c: InstalledContent): Boolean {
        val contentsDir = File(context.filesDir, "contents")
        val contentsCanonical = contentsDir.canonicalPath
        val dirCanonical = c.dir.canonicalPath
        if (!dirCanonical.startsWith(contentsCanonical + File.separator)) {
            return false
        }
        return try {
            deleteTree(c.dir)
            !c.dir.exists()
        } catch (_: Exception) {
            false
        }
    }

    suspend fun download(
        context: Context,
        url: String,
        expectedSha256: String?,
        onProgress: (Long) -> Unit = {}
    ): Result<Pair<File, String>> = withContext(Dispatchers.IO) {
        var currentUrl = try {
            URL(url)
        } catch (e: Exception) {
            return@withContext Result.failure(e)
        }

        if (currentUrl.protocol != "https") {
            return@withContext Result.failure(IllegalArgumentException("Only https URLs are allowed: $url"))
        }

        val outputFile = File(context.cacheDir, "dl_${System.nanoTime()}.wcp")
        var conn: HttpURLConnection? = null

        try {
            var redirectCount = 0
            while (redirectCount < 5) {
                val c = currentUrl.openConnection() as HttpURLConnection
                c.instanceFollowRedirects = true
                c.connectTimeout = 15000
                c.readTimeout = 30000
                c.connect()

                if (c.url.protocol != "https") {
                    c.disconnect()
                    throw SecurityException("Connection protocol is not https: ${c.url}")
                }

                val code = c.responseCode
                if (code in 301..308 && code != 304 && code != 305 && code != 306) {
                    val location = c.getHeaderField("Location")
                    c.disconnect()
                    if (location == null) {
                        throw IOException("HTTP redirect $code without Location header")
                    }
                    val nextUrl = URL(currentUrl, location)
                    if (nextUrl.protocol != "https") {
                        throw SecurityException("Redirect to non-https URL: $nextUrl")
                    }
                    currentUrl = nextUrl
                    redirectCount++
                    continue
                }
                conn = c
                break
            }

            val activeConn = conn ?: throw IOException("Failed to establish HTTP connection")
            if (activeConn.url.protocol != "https") {
                throw SecurityException("Active connection protocol is not https: ${activeConn.url}")
            }
            if (activeConn.responseCode !in 200..299) {
                throw IOException("HTTP error ${activeConn.responseCode}: ${activeConn.responseMessage}")
            }

            val digest = MessageDigest.getInstance("SHA-256")
            var totalBytes = 0L
            val maxBytes = 2L * 1024L * 1024L * 1024L // 2 GiB

            activeConn.inputStream.use { input ->
                FileOutputStream(outputFile).use { fos ->
                    val buffer = ByteArray(32768)
                    var bytesRead: Int
                    while (input.read(buffer).also { bytesRead = it } != -1) {
                        if (bytesRead > 0) {
                            totalBytes += bytesRead
                            if (totalBytes > maxBytes) {
                                throw SecurityException("Download size exceeded 2 GiB limit")
                            }
                            digest.update(buffer, 0, bytesRead)
                            fos.write(buffer, 0, bytesRead)
                            onProgress(totalBytes)
                        }
                    }
                }
            }

            val hexSha256 = digest.digest().joinToString("") { "%02x".format(it) }
            if (expectedSha256 != null && !hexSha256.equals(expectedSha256, ignoreCase = true)) {
                outputFile.delete()
                return@withContext Result.failure(
                    SecurityException("SHA-256 mismatch! Expected: $expectedSha256, got: $hexSha256")
                )
            }

            Result.success(Pair(outputFile, hexSha256))
        } catch (t: Throwable) {
            if (outputFile.exists()) {
                outputFile.delete()
            }
            Result.failure(t)
        } finally {
            conn?.disconnect()
        }
    }

    suspend fun copyFromUri(context: Context, uri: Uri): Result<Pair<File, String>> = withContext(Dispatchers.IO) {
        val outputFile = File(context.cacheDir, "dl_${System.nanoTime()}.wcp")
        val digest = MessageDigest.getInstance("SHA-256")
        var totalBytes = 0L
        val maxBytes = 2L * 1024L * 1024L * 1024L // 2 GiB

        try {
            context.contentResolver.openInputStream(uri)?.use { input ->
                FileOutputStream(outputFile).use { fos ->
                    val buffer = ByteArray(32768)
                    var bytesRead: Int
                    while (input.read(buffer).also { bytesRead = it } != -1) {
                        if (bytesRead > 0) {
                            totalBytes += bytesRead
                            if (totalBytes > maxBytes) {
                                throw SecurityException("File size exceeded 2 GiB limit")
                            }
                            digest.update(buffer, 0, bytesRead)
                            fos.write(buffer, 0, bytesRead)
                        }
                    }
                }
            } ?: throw IOException("Could not open input stream for URI: $uri")

            val hexSha256 = digest.digest().joinToString("") { "%02x".format(it) }
            Result.success(Pair(outputFile, hexSha256))
        } catch (t: Throwable) {
            if (outputFile.exists()) {
                outputFile.delete()
            }
            Result.failure(t)
        }
    }

    fun extractTar(archive: File, destDir: File, lenient: Boolean = false) {
        val destCanonical = destDir.canonicalPath
        val destPrefix = destCanonical + File.separator

        val fis = FileInputStream(archive)
        val bis = BufferedInputStream(fis)
        try {
            bis.mark(16)
            val magic = ByteArray(6)
            val bytesRead = bis.read(magic)
            bis.reset()
            if (bytesRead < 4) {
                throw IOException("Archive file is too small to identify format")
            }

            val isZstd = bytesRead >= 4 &&
                    magic[0] == 0x28.toByte() &&
                    magic[1] == 0xB5.toByte() &&
                    magic[2] == 0x2F.toByte() &&
                    magic[3] == 0xFD.toByte()

            val isXz = bytesRead >= 6 &&
                    magic[0] == 0xFD.toByte() &&
                    magic[1] == 0x37.toByte() &&
                    magic[2] == 0x7A.toByte() &&
                    magic[3] == 0x58.toByte() &&
                    magic[4] == 0x5A.toByte() &&
                    magic[5] == 0x00.toByte()

            val decompressor: InputStream = when {
                isZstd -> ZstdInputStream(bis)
                isXz -> XZInputStream(bis, 256 * 1024)
                else -> throw IllegalArgumentException("Unsupported archive compression format: unknown magic bytes")
            }

            var entryCount = 0
            var totalBytes = 0L
            val maxBytes = 3L * 1024L * 1024L * 1024L // 3 GiB
            val maxEntries = 50000

            TarArchiveInputStream(decompressor).use { tarIn ->
                var entry = tarIn.nextTarEntry
                while (entry != null) {
                    entryCount++
                    if (entryCount > maxEntries) {
                        throw SecurityException("Archive contains too many entries (limit $maxEntries)")
                    }

                    var name = entry.name
                    while (name.startsWith("./")) {
                        name = name.removePrefix("./")
                    }

                    if (name.isEmpty() || name == ".") {
                        if (entry.isDirectory) {
                            entry = tarIn.nextTarEntry
                            continue
                        } else {
                            throw SecurityException("Invalid entry name: ${entry.name}")
                        }
                    }

                    if (name.startsWith("/") || File(name).isAbsolute) {
                        throw SecurityException("Absolute path in archive: ${entry.name}")
                    }

                    val target = File(destDir, name)
                    val targetCanonical = target.canonicalPath
                    if (!targetCanonical.startsWith(destPrefix)) {
                        throw SecurityException("Path traversal / symlink escape detected for entry: ${entry.name}")
                    }

                    when {
                        entry.isSymbolicLink -> {
                            val linkName = entry.linkName
                            if (linkName.isNullOrEmpty()) {
                                throw SecurityException("Empty symlink target for ${entry.name}")
                            }
                            if (linkName.startsWith("/") || File(linkName).isAbsolute) {
                                if (lenient) {
                                    // (a) symlink entries whose target is absolute are SKIPPED (not created, not an error)
                                } else {
                                    throw SecurityException("Absolute symlink target for ${entry.name}: $linkName")
                                }
                            } else {
                                val parent = target.parentFile ?: destDir
                                parent.mkdirs()
                                val lexicalTarget = File(parent, linkName).toPath().normalize()
                                val destPath = destDir.toPath().normalize()
                                if (!lexicalTarget.startsWith(destPath)) {
                                    throw SecurityException("Symlink target escapes stage: ${entry.name} -> $linkName")
                                }
                                try {
                                    Files.deleteIfExists(target.toPath())
                                } catch (_: Exception) {}
                                android.system.Os.symlink(linkName, target.path)
                            }
                        }
                        entry.isDirectory -> {
                            target.mkdirs()
                        }
                        entry.isLink || entry.isCharacterDevice || entry.isBlockDevice || entry.isFIFO ->
                            throw SecurityException("Unsupported entry type (hardlink/device/fifo): ${entry.name}")
                        entry.isFile -> {
                            target.parentFile?.mkdirs()
                            try {
                                if (Files.isSymbolicLink(target.toPath())) {
                                    Files.delete(target.toPath())
                                }
                            } catch (_: Exception) {}
                            FileOutputStream(target).use { fos ->
                                val buffer = ByteArray(32768)
                                var count: Int
                                while (tarIn.read(buffer).also { count = it } != -1) {
                                    if (count > 0) {
                                        totalBytes += count
                                        if (totalBytes > maxBytes) {
                                            throw SecurityException("Archive extracted size exceeds 3 GiB limit")
                                        }
                                        fos.write(buffer, 0, count)
                                    }
                                }
                            }
                            if ((entry.mode and 0b001_001_001) != 0) {
                                target.setExecutable(true, false)
                            }
                        }
                        else -> {
                            throw SecurityException("Unsupported entry type in archive: ${entry.name}")
                        }
                    }

                    entry = tarIn.nextTarEntry
                }
            }
        } finally {
            try { bis.close() } catch (_: Exception) {}
        }

        // Post-pass: walk destDir and ensure all symlinks resolve under destDir
        val dangling = mutableListOf<java.nio.file.Path>()
        Files.walk(destDir.toPath()).use { stream ->
            stream.forEach { path ->
                if (Files.isSymbolicLink(path)) {
                    val file = path.toFile()
                    val targetExists = Files.exists(path)
                    if (targetExists) {
                        val canon = file.canonicalPath
                        if (canon != destCanonical && !canon.startsWith(destPrefix)) {
                            throw SecurityException("Symlink chain escapes stage: $path -> $canon")
                        }
                    } else {
                        if (lenient) {
                            dangling.add(path)
                        } else {
                            throw SecurityException("Dangling symlink: $path -> ${Files.readSymbolicLink(path)}")
                        }
                    }
                }
            }
        }
        for (p in dangling) {
            try {
                Files.deleteIfExists(p)
            } catch (_: Exception) {}
        }
    }

    suspend fun install(
        context: Context,
        archive: File,
        rootfs: Boolean = false
    ): Result<InstalledContent> = withContext(Dispatchers.IO) {
        val stageDir = File(context.filesDir, "staging/stage_${System.nanoTime()}")
        if (!stageDir.mkdirs()) {
            return@withContext Result.failure(IOException("Failed to create stage directory: ${stageDir.absolutePath}"))
        }

        try {
            extractTar(archive, stageDir, lenient = rootfs)

            if (rootfs) {
                val profileFile = File(stageDir, "profile.json")
                if (!profileFile.exists()) {
                    profileFile.writeText("""{"type":"imagefs","versionName":"bionic","description":"GameNative imagefs_bionic rootfs"}""")
                }
            }

            // Read profile.json
            val profileFile = File(stageDir, "profile.json")
            if (!profileFile.isFile) {
                throw IllegalArgumentException("profile.json not found in archive")
            }
            if (profileFile.length() > 64 * 1024L) {
                throw IllegalArgumentException("profile.json exceeds 64 KiB limit")
            }

            val json = JSONObject(profileFile.readText())
            val rawType = json.optString("type", "")
            val rawVersion = json.optString("versionName", "")
            val description = json.optString("description", "")

            val type = rawType.replace(Regex("[^A-Za-z0-9._-]"), "_")
            val versionName = rawVersion.replace(Regex("[^A-Za-z0-9._-]"), "_")

            if (type.isEmpty() || type == "." || type == "..") {
                throw IllegalArgumentException("Invalid type in profile.json: '$rawType'")
            }
            if (versionName.isEmpty() || versionName == "." || versionName == "..") {
                throw IllegalArgumentException("Invalid versionName in profile.json: '$rawVersion'")
            }

            val contentsDir = File(context.filesDir, "contents")
            val typeDir = File(contentsDir, type)
            val dest = File(typeDir, versionName)

            if (dest.exists()) {
                throw IllegalStateException("already installed, delete first")
            }

            dest.parentFile?.mkdirs()
            if (!stageDir.renameTo(dest)) {
                throw IOException("Failed to move staged contents to destination: ${dest.absolutePath}")
            }

            Result.success(
                InstalledContent(
                    type = type,
                    versionName = versionName,
                    description = description,
                    dir = dest
                )
            )
        } catch (t: Throwable) {
            try {
                deleteTree(stageDir)
            } catch (_: Exception) {}
            Result.failure(t)
        }
    }
}
