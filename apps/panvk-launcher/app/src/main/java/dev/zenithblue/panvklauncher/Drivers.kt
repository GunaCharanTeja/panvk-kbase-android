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
    val bundled: Boolean,
    val buildId: String = "",
    val sha256: String = "",
    val driverVersion: String = ""
) {
    /** "PanVK Kbase G615 - beta.9 (...)" style one-liner for dropdowns and the launch card. */
    val label: String get() = if (version.isEmpty()) name else "$name — $version"
}

object DriverManager {

    /** Id used before the pinned beta.9 driver was bundled; mapped to the current bundled id. */
    const val LEGACY_BUNDLED_ID = "bundled"

    private fun bundledMeta(context: Context): JSONObject? = try {
        JSONObject(context.assets.open("bundled-driver.json").bufferedReader().use { it.readText() })
    } catch (_: Exception) {
        null
    }

    fun bundledId(context: Context): String = bundledMeta(context)?.optString("id", "") ?: ""

    fun bundledDriver(context: Context): Driver {
        val m = bundledMeta(context)
        val lib = context.applicationInfo.nativeLibraryDir + "/libvulkan_panfrost.so"
        if (m == null) {
            return Driver(LEGACY_BUNDLED_ID, "PanVK (bundled)", "Bundled Panfrost Vulkan driver", "Mesa / PanVK", "unknown", lib, true)
        }
        val commit = m.optString("sourceCommit", "")
        val series = m.optString("patchSeriesId", "").removePrefix("sha256:")
        val rel = m.optJSONObject("release")
        val build = buildString {
            if (commit.isNotEmpty()) append("Mesa ").append(commit.take(8))
            if (series.isNotEmpty()) append(" · series ").append(series.take(8))
        }
        return Driver(
            id = m.optString("id", LEGACY_BUNDLED_ID),
            name = m.optString("name", "PanVK"),
            description = m.optString("description", ""),
            author = m.optString("author", "panvk-kbase-android"),
            version = m.optString("displayVersion", m.optString("packageVersion", "")),
            libPath = lib,
            bundled = true,
            buildId = build,
            sha256 = rel?.optString("sha256", "") ?: "",
            driverVersion = m.optString("driverVersion", "")
        )
    }

    /** One-time: old unnamed "bundled" selection and shortcut references move to the beta.9 bundled driver. */
    private fun migrateLegacy(context: Context) {
        val newId = bundledId(context)
        if (newId.isEmpty()) return
        val prefs = context.getSharedPreferences("launcher", Context.MODE_PRIVATE)
        if (prefs.getString("driver_migrated", "") == newId) return
        if (prefs.getString("driver", LEGACY_BUNDLED_ID) == LEGACY_BUNDLED_ID) {
            prefs.edit().putString("driver", newId).apply()
        }
        File(context.filesDir, "shortcuts").listFiles { f -> f.extension == "json" }?.forEach { f ->
            try {
                val j = JSONObject(f.readText())
                if (j.optString("driver", "") == LEGACY_BUNDLED_ID) {
                    j.put("driver", newId)
                    f.writeText(j.toString())
                }
            } catch (_: Exception) {
            }
        }
        prefs.edit().putString("driver_migrated", newId).apply()
    }

    /** Resolve a possibly legacy id ("bundled") to a driver in [drivers]. */
    fun find(context: Context, drivers: List<Driver>, id: String): Driver? {
        val want = if (id == LEGACY_BUNDLED_ID) bundledId(context).ifEmpty { id } else id
        return drivers.firstOrNull { it.id == want }
    }

    fun getDrivers(context: Context): List<Driver> {
        migrateLegacy(context)
        val list = mutableListOf<Driver>()

        // 1. Bundled PanVK driver (pinned release, see assets/bundled-driver.json)
        list.add(bundledDriver(context))

        // 2. Imported drivers from filesDir/drivers/<dirname>/
        val driversDir = File(context.filesDir, "drivers")
        if (driversDir.exists() && driversDir.isDirectory) {
            val subDirs = driversDir.listFiles()?.filter { it.isDirectory } ?: emptyList()
            for (dir in subDirs) {
                if (dir.name == "bundled" || dir.name == bundledId(context) || dir.name.startsWith(".")) continue
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
                            bundled = false,
                            buildId = buildIdOf(json),
                            driverVersion = drvVer
                        )
                    )
                } catch (_: Exception) {
                    // Ignore corrupted or unparseable metadata
                }
            }
        }

        return list
    }

    private fun buildIdOf(meta: JSONObject): String {
        val commit = meta.optString("sourceCommit", "")
        return if (commit.isEmpty()) "" else "Mesa " + commit.take(8)
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
            if (sanitized == "bundled" || sanitized == bundledId(context) || sanitized == "."|| sanitized == ".." || sanitized.startsWith(".")) {
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
                bundled = false,
                buildId = buildIdOf(metaJson),
                driverVersion = drvVer
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
        val id = prefs.getString("driver", LEGACY_BUNDLED_ID) ?: LEGACY_BUNDLED_ID
        return if (id == LEGACY_BUNDLED_ID) bundledId(context).ifEmpty { id } else id
    }

    fun setSelectedDriverId(context: Context, id: String) {
        val prefs = context.getSharedPreferences("launcher", Context.MODE_PRIVATE)
        val resolved = if (id == LEGACY_BUNDLED_ID) bundledId(context).ifEmpty { id } else id
        prefs.edit().putString("driver", resolved).apply()
    }

    fun getSelectedDriver(context: Context, drivers: List<Driver>): Driver {
        val match = find(context, drivers, getSelectedDriverId(context))
        if (match != null) {
            return match
        }
        // Fall back to bundled
        val bundled = drivers.firstOrNull { it.bundled } ?: bundledDriver(context)
        setSelectedDriverId(context, bundled.id)
        return bundled
    }
}
