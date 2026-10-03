package dev.zenithblue.panvklauncher

import android.app.Activity
import android.content.Context
import com.winlator.sysvshm.SysVSHMConnectionHandler
import com.winlator.sysvshm.SysVSHMRequestHandler
import com.winlator.sysvshm.SysVSharedMemory
import com.winlator.xconnector.UnixSocketConfig
import com.winlator.xconnector.XConnectorEpoll
import com.winlator.xserver.SHMSegmentManager
import com.winlator.xserver.ScreenInfo
import com.winlator.xserver.XClientConnectionHandler
import com.winlator.xserver.XClientRequestHandler
import com.winlator.xserver.XServer
import java.io.File
import java.lang.ref.WeakReference

/**
 * In-process X server (Winlator's Java X server, LGPL-2.1, see third_party/winlator).
 * One instance per graphical Wine run. Listens on `<imagefs>/usr/tmp/.X11-unix/X0`,
 * which is the directory Wine's TMPDIR already points at. MIT-SHM backed by a
 * SysV shm broker socket (`ANDROID_SYSVSHM_SERVER`) so Wine/libxcb can hand us
 * pixel buffers without pushing them through the socket.
 */
object BuiltinXServer {
    private const val X_SOCKET = "/tmp/.X11-unix/X0"
    private const val SHM_SOCKET = "/tmp/.sysvshm/SM0"
    const val DEFAULT_RESOLUTION = "1280x720"
    val RESOLUTIONS = listOf("800x600", "1280x720", "1600x900", "1920x1080")

    private val lock = Any()

    @Volatile
    var xServer: XServer? = null
        private set
    private var xConnector: XConnectorEpoll? = null
    private var shmConnector: XConnectorEpoll? = null
    private var shm: SysVSharedMemory? = null
    private var activity: WeakReference<Activity>? = null
    private var dumping = false

    fun isRunning(): Boolean = xServer != null

    fun usrDir(ctx: Context): File = File(ctx.filesDir, "contents/imagefs/bionic/usr")

    fun start(ctx: Context, resolution: String, useShm: Boolean, onLine: (String) -> Unit): DisplayServer.Session? =
        synchronized(lock) {
            stopLocked()
            killStaleTermuxX11(onLine)
            val usr = usrDir(ctx)
            if (!File(usr, "tmp").isDirectory && !File(usr, "tmp").mkdirs()) {
                onLine("Built-in X server: cannot create ${usr}/tmp")
                return null
            }
            val xs: XServer
            try {
                xs = XServer(ScreenInfo(resolution))
            } catch (t: Throwable) {
                onLine("Built-in X server init failed: $t")
                return null
            }
            com.winlator.core.XLog.open(File(ctx.filesDir, "xserver.log"))
            val env = linkedMapOf<String, String>()
            try {
                if (useShm) {
                    val sysv = SysVSharedMemory()
                    val shmCfg = UnixSocketConfig.createSocket(usr.absolutePath, SHM_SOCKET)
                    val c = XConnectorEpoll(shmCfg, SysVSHMConnectionHandler(sysv), SysVSHMRequestHandler())
                    c.start()
                    xs.setSHMSegmentManager(SHMSegmentManager(sysv))
                    shm = sysv
                    shmConnector = c
                    env["ANDROID_SYSVSHM_SERVER"] = shmCfg.path
                }
                val cfg = UnixSocketConfig.createSocket(usr.absolutePath, X_SOCKET)
                val c = XConnectorEpoll(cfg, XClientConnectionHandler(xs), XClientRequestHandler())
                c.setInitialInputBufferCapacity(262144)
                c.setCanReceiveAncillaryMessages(true)
                c.start()
                xConnector = c
                env["DISPLAY"] = ":0"
                env["TMPDIR"] = File(usr, "tmp").absolutePath
                xServer = xs
                GamepadBridge.start(ctx)
                if (!dumping) { dumping = true; startDumpThread(ctx) }
                onLine("Built-in X server $resolution on ${cfg.path}${if (useShm) " (MIT-SHM via SysV broker)" else ""}")
                DisplayServer.Session(":0", cfg.path, env, true)
            } catch (t: Throwable) {
                onLine("Built-in X server start failed: $t")
                stopLocked()
                null
            }
        }

    // A leaked Termux:X11 child we started in Termux mode still owns the abstract socket
    // @/tmp/.X11-unix/X0, and libX11 tries the abstract name first: Wine would then render
    // into that invisible server (black built-in display). Kill our own leftovers.
    private fun killStaleTermuxX11(onLine: (String) -> Unit) {
        val me = android.os.Process.myPid()
        File("/proc").listFiles()?.forEach { d ->
            val pid = d.name.toIntOrNull() ?: return@forEach
            if (pid == me) return@forEach
            try {
                val cmd = File(d, "cmdline").readBytes().toString(Charsets.UTF_8)
                if (cmd.contains("termux-x11")) {
                    onLine("Killing stale termux-x11 pid $pid")
                    android.os.Process.killProcess(pid)
                }
            } catch (_: Exception) {}
        }
    }

    fun stop() = synchronized(lock) { stopLocked() }

    private fun stopLocked() {
        val a = activity?.get()
        activity = null
        xConnector?.stop()
        xConnector = null
        shmConnector?.stop()
        shmConnector = null
        shm?.deleteAll()
        shm = null
        xServer?.cursorLocker?.stop()
        if (xServer != null) GamepadBridge.stop()
        xServer = null
        a?.runOnUiThread { a.finish() }
    }

    /** Window tree + pixel stats, for debugging blank displays. adb: run-as ... cat files/xdump.txt */
    fun dump(): String {
        val xs = xServer ?: return "no server"
        val sb = StringBuilder("renderer: ${xs.renderer?.debugState()}\n")
        fun walk(w: com.winlator.xserver.Window, depth: Int) {
            val d = w.content
            var nz = 0
            val data = d.data
            if (data != null) {
                val n = data.capacity()
                var i = 0
                val step = maxOf(1, n / 4096)
                var seen = 0
                while (i < n) { seen++; if (data.get(i).toInt() != 0) nz++; i += step }
                nz = if (seen > 0) nz * 100 / seen else 0
            }
            sb.append("  ".repeat(depth)).append("win ${w.id} ${w.x},${w.y} ${w.width}x${w.height} mapped=${w.attributes.isMapped} ")
                .append("cls=${w.className} name=${w.name} content=${d.width}x${d.height} nonzero%=$nz\n")
            for (c in w.children) walk(c, depth + 1)
        }
        walk(xs.windowManager.rootWindow, 0)
        return sb.toString()
    }

    fun startDumpThread(ctx: Context) {
        val f = File(ctx.filesDir, "xdump.txt")
        Thread({
            while (true) {
                try { f.writeText(dump()) } catch (_: Throwable) {}
                try { Thread.sleep(2000) } catch (_: InterruptedException) { break }
            }
        }, "xdump").apply { isDaemon = true }.start()
    }

    fun attach(a: Activity) = synchronized(lock) { activity = WeakReference(a) }

    fun detach(a: Activity) = synchronized(lock) {
        if (activity?.get() === a) activity = null
    }
}
