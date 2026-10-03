package dev.zenithblue.panvklauncher

import android.content.Context
import android.content.pm.ApplicationInfo
import android.graphics.Bitmap
import androidx.compose.runtime.mutableStateOf
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.RandomAccessFile
import java.util.UUID

/**
 * Game shortcut = files/shortcuts/<id>.json (+ <id>.png icon). Edited by the Games tab or
 * written directly over adb (tools/launch-shortcut.py --add); see docs/GAME-SHORTCUTS.md.
 * [icon]: "auto" = not extracted yet, "none" = no usable icon, else icon file name.
 */
data class Shortcut(
    val id: String,
    val name: String,
    val exe: String,
    val args: String = "",
    val env: Map<String, String> = mapOf("DXVK_HUD" to "full"),
    val arch: String = "auto", // auto | i386 | x86_64 | arm64ec | arm64
    val resolution: String = "", // "" = launcher default, else e.g. 1280x720
    val driver: String = "", // "" = launcher default, else Driver.id
    val icon: String = "auto",
    val created: Long = System.currentTimeMillis(),
    val lastPlayed: Long = 0
)

/** Per-launch overrides threaded into ContainerManager.env() / runExe(). */
data class LaunchOptions(
    val args: List<String> = emptyList(),
    val env: Map<String, String> = emptyMap(),
    val driverId: String = ""
)

/** Intent extra consumed by MainActivity (debug builds only): shortcut id or name. */
const val EXTRA_LAUNCH_SHORTCUT = "dev.zenithblue.panvklauncher.LAUNCH_SHORTCUT"

/** Pending launch request from an intent; consumed by LauncherApp. */
object ShortcutRequests {
    val pending = mutableStateOf<String?>(null)

    fun fromIntent(ctx: Context, intent: android.content.Intent?) {
        val v = intent?.getStringExtra(EXTRA_LAUNCH_SHORTCUT) ?: return
        // Exported activity: only honour the extra in debuggable builds.
        if ((ctx.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE) == 0) return
        pending.value = v
    }
}

object ShortcutStore {
    private fun dir(ctx: Context) = File(ctx.filesDir, "shortcuts").apply { mkdirs() }
    fun iconFile(ctx: Context, s: Shortcut): File? =
        if (s.icon == "auto" || s.icon == "none") null else File(dir(ctx), s.icon).takeIf { it.isFile }

    fun newId() = "g" + UUID.randomUUID().toString().replace("-", "").take(8)

    private fun toJson(s: Shortcut) = JSONObject().apply {
        put("id", s.id); put("name", s.name); put("exe", s.exe); put("args", s.args)
        put("env", JSONObject(s.env)); put("arch", s.arch); put("resolution", s.resolution)
        put("driver", s.driver); put("icon", s.icon); put("created", s.created)
        put("lastPlayed", s.lastPlayed)
    }

    private fun fromJson(j: JSONObject): Shortcut {
        val e = j.optJSONObject("env")
        val env = mutableMapOf<String, String>()
        e?.keys()?.forEach { env[it] = e.optString(it) }
        return Shortcut(
            id = j.getString("id"), name = j.optString("name", j.getString("id")),
            exe = j.getString("exe"), args = j.optString("args", ""), env = env,
            arch = j.optString("arch", "auto"), resolution = j.optString("resolution", ""),
            driver = j.optString("driver", ""), icon = j.optString("icon", "auto"),
            created = j.optLong("created", 0), lastPlayed = j.optLong("lastPlayed", 0)
        )
    }

    fun list(ctx: Context): List<Shortcut> =
        (dir(ctx).listFiles { f -> f.isFile && f.name.endsWith(".json") } ?: emptyArray())
            .mapNotNull { f -> try { fromJson(JSONObject(f.readText())) } catch (_: Exception) { null } }
            .sortedBy { it.name.lowercase() }

    /** Match by exact id, else case-insensitive name, else unique name prefix. Includes built-in tests. */
    fun find(ctx: Context, key: String): Shortcut? {
        val all = list(ctx) + BuiltinTests.list(ctx)
        all.firstOrNull { it.id == key }?.let { return it }
        all.firstOrNull { it.name.equals(key, true) }?.let { return it }
        return all.filter { it.name.startsWith(key, true) }.singleOrNull()
    }

    fun save(ctx: Context, s: Shortcut) {
        val f = File(dir(ctx), "${s.id}.json")
        val tmp = File(dir(ctx), "${s.id}.json.tmp")
        tmp.writeText(toJson(s).toString(2))
        tmp.renameTo(f)
    }

    fun delete(ctx: Context, s: Shortcut) {
        File(dir(ctx), "${s.id}.json").delete()
        File(dir(ctx), "${s.id}.png").delete()
        ControllerConfig.file(ctx, s.id).delete()
    }

    fun duplicate(ctx: Context, s: Shortcut): Shortcut {
        val id = newId()
        var icon = s.icon
        iconFile(ctx, s)?.let { src ->
            val dst = File(dir(ctx), "$id.png")
            src.copyTo(dst, overwrite = true)
            icon = dst.name
        }
        ControllerConfig.file(ctx, s.id).takeIf { it.isFile }?.copyTo(ControllerConfig.file(ctx, id), overwrite = true)
        val copy = s.copy(id = id, name = s.name + " (copy)", icon = icon, created = System.currentTimeMillis(), lastPlayed = 0)
        save(ctx, copy)
        return copy
    }

    /** Fill in arch / icon for shortcuts created without them (CLI). Returns updated shortcut. */
    fun ensureMeta(ctx: Context, s: Shortcut): Shortcut {
        var n = s
        val exe = resolveExe(ctx, s.exe)
        if (n.arch == "auto") PeInfo.arch(exe)?.let { n = n.copy(arch = it) }
        if (n.icon == "auto") {
            val png = PeInfo.iconPng(exe)
            n = if (png != null) {
                File(dir(ctx), "${n.id}.png").writeBytes(png)
                n.copy(icon = "${n.id}.png")
            } else n.copy(icon = "none")
        }
        if (n != s) save(ctx, n)
        return n
    }

    /** exe may be an Android path or a Wine path under the container C: drive. */
    fun resolveExe(ctx: Context, exe: String): String {
        val m = Regex("^([A-Za-z]):[\\\\/](.*)$").matchEntire(exe) ?: return exe
        if (!m.groupValues[1].equals("c", true)) return exe
        return File(ctx.filesDir, "container/.wine/drive_c/" + m.groupValues[2].replace('\\', '/')).path
    }

    fun launchOptions(ctx: Context, s: Shortcut) = LaunchOptions(
        args = splitArgs(s.args), env = s.env, driverId = s.driver
    )

    /** Whitespace split honouring "double quotes". */
    fun splitArgs(s: String): List<String> =
        Regex("\"([^\"]*)\"|(\\S+)").findAll(s).map { it.groups[1]?.value ?: it.groups[2]!!.value }.toList()

    /** "KEY=VALUE" per line (blank / # lines ignored) <-> map. */
    fun parseEnv(text: String): Map<String, String> = text.lines().map { it.trim() }
        .filter { it.isNotEmpty() && !it.startsWith("#") && it.contains('=') }
        .associate { it.substringBefore('=').trim() to it.substringAfter('=') }
        .filterKeys { it.isNotEmpty() }

    fun envText(env: Map<String, String>) = env.entries.joinToString("\n") { "${it.key}=${it.value}" }
}

/** Minimal PE reader: machine type and first icon (PNG or DIB entry) from the resource section. */
object PeInfo {
    private class Pe(val f: RandomAccessFile) {
        fun u16(o: Long): Int { f.seek(o); return f.read() or (f.read() shl 8) }
        fun u32(o: Long): Long { f.seek(o); var v = 0L; for (i in 0 until 4) v = v or (f.read().toLong() shl (8 * i)); return v }
        fun bytes(o: Long, n: Int): ByteArray { f.seek(o); val b = ByteArray(n); f.readFully(b); return b }
        val peOff = u32(0x3C)
        val machine = u16(peOff + 4)
        val nSec = u16(peOff + 6)
        val optOff = peOff + 24
        val plus = u16(optOff) == 0x20b
        val secOff = optOff + u16(peOff + 20)
        fun rvaToOff(rva: Long): Long {
            for (i in 0 until nSec) {
                val s = secOff + i * 40L
                val va = u32(s + 12); val raw = u32(s + 20)
                val size = maxOf(u32(s + 8), u32(s + 16))
                if (rva >= va && rva < va + size) return raw + (rva - va)
            }
            return -1
        }
    }

    fun arch(path: String): String? = try {
        RandomAccessFile(path, "r").use { f ->
            val pe = Pe(f)
            if (pe.u32(pe.peOff) != 0x4550L) null
            else when (pe.machine) {
                0x14c -> "i386"; 0xA641 -> "arm64ec"; 0xAA64 -> "arm64"
                // ARM64EC images carry the AMD64 machine id; they differ by a non-null CHPEMetadataPointer
                // (load config dir, offset 200).
                0x8664 -> {
                    val lc = pe.rvaToOff(pe.u32(pe.optOff + 112 + 10 * 8L))
                    if (lc > 0 && pe.u32(lc) >= 208 && (pe.u32(lc + 200) != 0L || pe.u32(lc + 204) != 0L)) "arm64ec" else "x86_64"
                }
                else -> null
            }
        }
    } catch (_: Throwable) { null }

    fun iconPng(path: String): ByteArray? = try {
        RandomAccessFile(path, "r").use { f -> extract(Pe(f)) }
    } catch (_: Throwable) { null }

    // Resource directory node at [dir]; returns (id, offsetField) entries.
    private fun entries(pe: Pe, dir: Long): List<Pair<Long, Long>> {
        val n = pe.u16(dir + 12) + pe.u16(dir + 14)
        return (0 until n).map { pe.u32(dir + 16 + it * 8L) to pe.u32(dir + 20 + it * 8L) }
    }

    /** Follow first child down to a data entry; returns bytes. [id]=null: first entry. */
    private fun leaf(pe: Pe, root: Long, rsrcRva: Long, node: Long, id: Long?): ByteArray? {
        var cur = node
        var want = id
        repeat(3) {
            val e = entries(pe, cur).let { l -> if (want != null) l.firstOrNull { it.first == want } else l.firstOrNull() } ?: return null
            want = null
            if (e.second and 0x80000000L != 0L) cur = root + (e.second and 0x7fffffffL)
            else {
                val d = root + e.second
                val off = pe.rvaToOff(pe.u32(d)); val size = pe.u32(d + 4).toInt()
                if (off < 0 || size <= 0 || size > 4_000_000) return null
                return pe.bytes(off, size)
            }
        }
        return null
    }

    private fun typeDir(pe: Pe, root: Long, type: Long): Long? =
        entries(pe, root).firstOrNull { it.first == type && it.second and 0x80000000L != 0L }
            ?.let { root + (it.second and 0x7fffffffL) }

    private fun extract(pe: Pe): ByteArray? {
        if (pe.u32(pe.peOff) != 0x4550L) return null
        val rsrcRva = pe.u32(pe.optOff + (if (pe.plus) 112 else 96) + 16)
        if (rsrcRva == 0L) return null
        val root = pe.rvaToOff(rsrcRva)
        if (root < 0) return null
        val grpDir = typeDir(pe, root, 14) ?: return null
        val grp = leaf(pe, root, rsrcRva, grpDir, null) ?: return null
        val count = (grp[4].toInt() and 0xff) or ((grp[5].toInt() and 0xff) shl 8)
        var best: Triple<Int, Int, Long>? = null // (score, w, id)
        for (i in 0 until count) {
            val o = 6 + i * 14
            if (o + 14 > grp.size) break
            val w = (grp[o].toInt() and 0xff).let { if (it == 0) 256 else it }
            val bpp = (grp[o + 6].toInt() and 0xff) or ((grp[o + 7].toInt() and 0xff) shl 8)
            val id = ((grp[o + 12].toInt() and 0xff) or ((grp[o + 13].toInt() and 0xff) shl 8)).toLong()
            // prefer <=128 px (cheap), then higher bpp
            val score = (if (w <= 128) w else 128 - w / 4) * 64 + bpp
            if (best == null || score > best.first) best = Triple(score, w, id)
        }
        val icoDir = typeDir(pe, root, 3) ?: return null
        val data = leaf(pe, root, rsrcRva, icoDir, best?.third ?: return null) ?: return null
        return toPng(data)
    }

    private fun toPng(d: ByteArray): ByteArray? {
        if (d.size > 8 && d[1] == 'P'.code.toByte() && d[2] == 'N'.code.toByte()) return d
        fun i32(o: Int) = (d[o].toInt() and 0xff) or ((d[o + 1].toInt() and 0xff) shl 8) or
            ((d[o + 2].toInt() and 0xff) shl 16) or ((d[o + 3].toInt() and 0xff) shl 24)
        val hdr = i32(0); val w = i32(4); val h = i32(8) / 2
        val bpp = (d[14].toInt() and 0xff) or ((d[15].toInt() and 0xff) shl 8)
        if (w <= 0 || h <= 0 || w > 512 || h > 512 || i32(16) != 0) return null
        val nPal = if (bpp <= 8) (i32(32).takeIf { it > 0 } ?: (1 shl bpp)) else 0
        val palOff = hdr
        val pixOff = hdr + nPal * 4
        val stride = ((w * bpp + 31) / 32) * 4
        val maskOff = pixOff + stride * h
        val mStride = ((w + 31) / 32) * 4
        val px = IntArray(w * h)
        var anyAlpha = false
        for (y in 0 until h) {
            val row = pixOff + (h - 1 - y) * stride
            for (x in 0 until w) {
                val argb = when (bpp) {
                    32 -> { val o = row + x * 4; (i32(o)).also { if (it ushr 24 != 0) anyAlpha = true } }
                    24 -> { val o = row + x * 3; (0xff shl 24) or ((d[o + 2].toInt() and 0xff) shl 16) or ((d[o + 1].toInt() and 0xff) shl 8) or (d[o].toInt() and 0xff) }
                    8, 4, 1 -> {
                        val bit = x * bpp
                        val b = d[row + bit / 8].toInt() and 0xff
                        val idx = (b shr (8 - bpp - bit % 8)) and ((1 shl bpp) - 1)
                        (0xff shl 24) or (i32(palOff + idx * 4) and 0xffffff)
                    }
                    else -> return null
                }
                px[y * w + x] = argb
            }
        }
        if (bpp != 32 || !anyAlpha) {
            for (y in 0 until h) {
                val row = maskOff + (h - 1 - y) * mStride
                if (row + mStride > d.size) break
                for (x in 0 until w) {
                    val masked = (d[row + x / 8].toInt() shr (7 - x % 8)) and 1
                    px[y * w + x] = if (masked == 1) 0 else px[y * w + x] or (0xff shl 24)
                }
            }
        }
        val bmp = Bitmap.createBitmap(px, w, h, Bitmap.Config.ARGB_8888)
        return ByteArrayOutputStream().also { bmp.compress(Bitmap.CompressFormat.PNG, 100, it) }.toByteArray()
    }
}

/** Own test programs (built from tests/dxcube.c) shipped in assets/builtin; read-only shortcuts, never stored in files/shortcuts. */
object BuiltinTests {
    private val ARCHES = listOf(Triple("i686", "i386", "x86"), Triple("x86_64", "x86_64", "x64"), Triple("arm64ec", "arm64ec", "ARM64EC"))
    // (api, label, exe prefix, argument)
    private val APIS = listOf(
        listOf("d3d8", "D3D8", "dxcube8", ""), listOf("d3d9", "D3D9", "dxcube", "d3d9"),
        listOf("d3d10", "D3D10", "dxcube", "d3d10"), listOf("d3d11", "D3D11", "dxcube", "d3d11")
    )

    fun isBuiltin(s: Shortcut) = s.id.startsWith("builtin-")
    private fun dir(ctx: Context) = File(ctx.filesDir, "games/builtin")

    /** Copy bundled exes out of the APK (first run, or when the APK ships a different size). */
    fun extract(ctx: Context) {
        val d = dir(ctx).apply { mkdirs() }
        val updated = try { ctx.packageManager.getPackageInfo(ctx.packageName, 0).lastUpdateTime } catch (_: Exception) { Long.MAX_VALUE }
        try {
            for (n in ctx.assets.list("builtin") ?: return) {
                val out = File(d, n)
                // Re-extract after each app update (compressed assets have no cheap length).
                if (out.isFile && out.lastModified() >= updated) continue
                ctx.assets.open("builtin/$n").use { i -> out.outputStream().use { i.copyTo(it) } }
            }
        } catch (_: Exception) {}
    }

    fun list(ctx: Context): List<Shortcut> = APIS.flatMap { (api, label, exe, arg) ->
        ARCHES.mapNotNull { (file, arch, short) ->
            val f = File(dir(ctx), "$exe-$file.exe")
            if (!f.isFile) null else Shortcut(
                id = "builtin-$api-$file", name = "Cube · $label · $short", exe = f.path, args = arg,
                arch = arch, icon = "res:ic_directx_cube", created = 0
            )
        }
    }
}
