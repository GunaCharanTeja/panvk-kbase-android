package dev.zenithblue.panvklauncher

import android.content.Context
import android.net.Uri
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.util.zip.ZipInputStream

data class Driver(
    val id: String,
    val name: String,
    val description: String,
    val author: String,
    val version: String,
    val libPath: String,
    val bundled: Boolean
)

object DriverManager {

    fun getDrivers(context: Context): List<Driver> {
        val list = mutableListOf<Driver>()

        // 1. Bundled PanVK driver
        val bundledLibPath = context.applicationInfo.nativeLibraryDir + "/libvulkan_panfrost.so"
        list.add(
            Driver(
                id = "bundled",
                name = "PanVK (bundled)",
                description = "Bundled Panfrost Vulkan driver",
                author = "Mesa / PanVK",
                version = "git",
                libPath = bundledLibPath,
                bundled = true
            )
        )

        // 2. Imported drivers from filesDir/drivers/<dirname>/
        val driversDir = File(context.filesDir, "drivers")
        if (driversDir.exists() && driversDir.isDirectory) {
            val subDirs = driversDir.listFiles()?.filter { it.isDirectory } ?: emptyList()
            for (dir in subDirs) {
                if (dir.name == "bundled" || dir.name.startsWith(".")) continue
                val metaFile = File(dir, "meta.json")
                if (!metaFile.isFile) continue
                if (metaFile.length() > 64 * 1024L) continue
                try {
                    val json = JSONObject(metaFile.readText())
                    val libraryName = json.optString("libraryName", "")
                    if (libraryName.isEmpty() || libraryName.contains('/') || libraryName.contains("..")) {
                        continue
                    }
                    val libFile = File(dir, libraryName)
                    if (!libFile.isFile) continue

                    val name = json.optString("name", dir.name)
                    val description = json.optString("description", "")
                    val author = json.optString("author", "Unknown")
                    val pkgVer = json.optString("packageVersion", "")
                    val drvVer = json.optString("driverVersion", "")
                    val version = if (pkgVer.isNotEmpty()) pkgVer else drvVer.ifEmpty { "unknown" }

                    list.add(
                        Driver(
                            id = dir.name,
                            name = name,
                            description = description,
                            author = author,
                            version = version,
                            libPath = libFile.absolutePath,
                            bundled = false
                        )
                    )
                } catch (_: Exception) {
                    // Ignore corrupted or unparseable metadata
                }
            }
        }

        return list
    }

    suspend fun importDriver(context: Context, uri: Uri): Result<Driver> = withContext(Dispatchers.IO) {
        val driversDir = File(context.filesDir, "drivers")
        if (!driversDir.exists()) {
            driversDir.mkdirs()
        }

        val tempDir = File(context.filesDir, "staging/import_${System.nanoTime()}")
        if (!tempDir.mkdirs()) {
            return@withContext Result.failure(IOException("Failed to create temporary directory for extraction"))
        }

        try {
            val destCanonical = tempDir.canonicalPath
            val destPrefix = destCanonical + File.separator

            var entryCount = 0
            var totalBytes = 0L
            val maxBytes = 1024L * 1024L * 1024L
            val maxEntries = 2048

            context.contentResolver.openInputStream(uri)?.use { rawIn ->
                ZipInputStream(rawIn).use { zis ->
                    var entry = zis.nextEntry
                    while (entry != null) {
                        entryCount++
                        if (entryCount > maxEntries) {
                            throw SecurityException("Archive contains too many entries (limit $maxEntries)")
                        }

                        val targetFile = File(tempDir, entry.name)
                        val targetCanonical = targetFile.canonicalPath
                        // SECURITY: reject zip-slip
                        if (!targetCanonical.startsWith(destPrefix)) {
                            throw SecurityException("Zip-slip detected for entry: ${entry.name}")
                        }

                        if (entry.isDirectory) {
                            targetFile.mkdirs()
                        } else {
                            targetFile.parentFile?.mkdirs()
                            FileOutputStream(targetFile).use { fos ->
                                val buffer = ByteArray(8192)
                                var bytesRead: Int
                                while (zis.read(buffer).also { bytesRead = it } != -1) {
                                    if (bytesRead > 0) {
                                        totalBytes += bytesRead
                                        if (totalBytes > maxBytes) {
                                            throw SecurityException("Archive extracted size exceeds 1 GiB limit")
                                        }
                                        fos.write(buffer, 0, bytesRead)
                                    }
                                }
                            }
                        }
                        zis.closeEntry()
                        entry = zis.nextEntry
                    }
                }
            } ?: throw IOException("Could not open input stream for Uri: $uri")

            val metaFile = File(tempDir, "meta.json")
            if (!metaFile.isFile) {
                throw IllegalArgumentException("meta.json not found in imported archive")
            }
            if (metaFile.length() > 64 * 1024L) {
                throw IllegalArgumentException("meta.json exceeds 64 KiB limit")
            }

            val metaJson = JSONObject(metaFile.readText())
            val libraryName = metaJson.optString("libraryName", "")
            if (libraryName.isEmpty() || libraryName.contains('/') || libraryName.contains("..")) {
                throw IllegalArgumentException("Invalid or missing libraryName in meta.json: '$libraryName'")
            }

            val libFile = File(tempDir, libraryName)
            if (!libFile.isFile) {
                throw IllegalArgumentException("Library file '$libraryName' specified in meta.json was not found in archive")
            }

            val rawName = metaJson.optString("name", "driver")
            var sanitized = rawName.replace(Regex("[^A-Za-z0-9._-]"), "_").ifEmpty { "driver" }
            if (sanitized == "bundled" || sanitized == "." || sanitized == ".." || sanitized.startsWith(".")) {
                sanitized = "drv_$sanitized"
            }

            var targetDir = File(driversDir, sanitized)
            var counter = 1
            while (targetDir.exists()) {
                targetDir = File(driversDir, "${sanitized}_$counter")
                counter++
            }

            if (!tempDir.renameTo(targetDir)) {
                try {
                    tempDir.copyRecursively(targetDir, overwrite = true)
                } catch (t: Throwable) {
                    try {
                        ContentManager.deleteTree(targetDir)
                    } catch (_: Exception) {}
                    throw t
                }
                try {
                    ContentManager.deleteTree(tempDir)
                } catch (_: Exception) {}
            }

            val finalLibFile = File(targetDir, libraryName)
            val name = metaJson.optString("name", targetDir.name)
            val description = metaJson.optString("description", "")
            val author = metaJson.optString("author", "Unknown")
            val pkgVer = metaJson.optString("packageVersion", "")
            val drvVer = metaJson.optString("driverVersion", "")
            val version = if (pkgVer.isNotEmpty()) pkgVer else drvVer.ifEmpty { "unknown" }

            val driver = Driver(
                id = targetDir.name,
                name = name,
                description = description,
                author = author,
                version = version,
                libPath = finalLibFile.absolutePath,
                bundled = false
            )
            Result.success(driver)
        } catch (t: Throwable) {
            try {
                ContentManager.deleteTree(tempDir)
            } catch (_: Exception) {}
            Result.failure(t)
        }
    }

    fun deleteDriver(context: Context, driver: Driver): Boolean {
        if (driver.bundled) return false
        val dir = File(context.filesDir, "drivers/${driver.id}")
        return if (dir.exists()) {
            try {
                ContentManager.deleteTree(dir)
                !dir.exists()
            } catch (_: Exception) {
                false
            }
        } else {
            false
        }
    }

    fun getSelectedDriverId(context: Context): String {
        val prefs = context.getSharedPreferences("launcher", Context.MODE_PRIVATE)
        return prefs.getString("driver", "bundled") ?: "bundled"
    }

    fun setSelectedDriverId(context: Context, id: String) {
        val prefs = context.getSharedPreferences("launcher", Context.MODE_PRIVATE)
        prefs.edit().putString("driver", id).apply()
    }

    fun getSelectedDriver(context: Context, drivers: List<Driver>): Driver {
        val selectedId = getSelectedDriverId(context)
        val match = drivers.firstOrNull { it.id == selectedId }
        if (match != null) {
            return match
        }
        // Fall back to bundled
        setSelectedDriverId(context, "bundled")
        return drivers.firstOrNull { it.id == "bundled" }
            ?: drivers.firstOrNull()
            ?: Driver(
                id = "bundled",
                name = "PanVK (bundled)",
                description = "Bundled Panfrost Vulkan driver",
                author = "Mesa / PanVK",
                version = "git",
                libPath = context.applicationInfo.nativeLibraryDir + "/libvulkan_panfrost.so",
                bundled = true
            )
    }
}
