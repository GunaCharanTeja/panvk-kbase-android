package dev.zenithblue.panvklauncher

/**
 * Wine's libX11 (stock or Termux xtrans) connects to a filesystem socket
 * `<dir>/.X11-unix/X<n>`. Termux:X11 listens on `$TMPDIR/.X11-unix/X<n>`.
 * The framebuffer file is not this socket.
 */
internal object LaunchGate {
    fun cancelled(stopRequested: Boolean, stopping: Boolean): Boolean =
        stopRequested || stopping
}

internal object DisplayPaths {
    fun socketFor(template: String, display: Int): String = template + display

    fun tmpdirOf(template: String): String {
        val unixDir = template.removeSuffix("/X")
        val slash = unixDir.lastIndexOf('/')
        return if (slash > 0) unixDir.substring(0, slash) else unixDir
    }

    fun tmpdirOfSocket(socketPath: String): String =
        tmpdirOf(socketPath.dropLastWhile { it.isDigit() })

    fun pick(templates: List<String>, isLive: (String) -> Boolean): Pair<Int, String>? {
        val unique = templates.distinct()
        for (display in 0..9) {
            for (template in unique) {
                val path = socketFor(template, display)
                if (isLive(path)) return display to path
            }
        }
        return null
    }
}

fun main() {
    check(!LaunchGate.cancelled(stopRequested = false, stopping = false))
    check(LaunchGate.cancelled(stopRequested = true, stopping = false))
    check(LaunchGate.cancelled(stopRequested = true, stopping = true))
    val termux = "/data/data/com.termux/files/usr/tmp/.X11-unix/X"
    val imagefs = "/data/user/0/dev.zenithblue.panvklauncher/files/contents/imagefs/bionic/usr/tmp/.X11-unix/X"
    check(DisplayPaths.socketFor("/tmp/.X11-unix/X", 0) == "/tmp/.X11-unix/X0")
    check(DisplayPaths.socketFor(termux, 1) == "$termux" + "1")
    check(DisplayPaths.tmpdirOf(termux) == "/data/data/com.termux/files/usr/tmp")
    check(DisplayPaths.tmpdirOfSocket("/tmp/.X11-unix/X0") == "/tmp")
    check(DisplayPaths.tmpdirOfSocket(imagefs + "0") == imagefs.removeSuffix("/.X11-unix/X"))
    val picked = DisplayPaths.pick(listOf(imagefs, "/tmp/.X11-unix/X")) { it == "/tmp/.X11-unix/X0" }
    check(picked == (0 to "/tmp/.X11-unix/X0"))
    check(DisplayPaths.tmpdirOfSocket(picked.second) == "/tmp")
    check(DisplayPaths.pick(listOf("/tmp/.X11-unix/X")) { false } == null)
    println("display-paths ok")
}
