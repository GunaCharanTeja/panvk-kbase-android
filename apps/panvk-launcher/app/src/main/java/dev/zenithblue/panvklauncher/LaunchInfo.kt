package dev.zenithblue.panvklauncher

import android.content.Context
import java.io.File

/** One line of the launch card: what it is, what is installed, and whether it is usable. */
data class LaunchItem(
    val label: String,
    val name: String,
    val version: String,
    val missing: Boolean = false
)

/**
 * Everything a launch will use, read from the real installed component metadata
 * (filesDir/contents/<type>/<ver>/profile.json, controller file) when the card opens.
 */
data class LaunchInfo(
    val rootfs: LaunchItem,
    val wine: LaunchItem,
    val fex: LaunchItem,
    val dxvk: LaunchItem,
    val dxvkEnabled: Boolean,
    val controller: LaunchItem,
    val exePath: String,
    val exeExists: Boolean,
    val arch: String
) {
    /** Components a game cannot start without. DXVK is optional (can be switched off). */
    val missing: List<LaunchItem> get() = listOf(rootfs, wine, fex).filter { it.missing }
}

object LaunchInfoResolver {
    /** [useDescription]: show the package's own profile.json description as its name (rootfs has no version number). */
    private fun item(ctx: Context, type: String, label: String, name: String, useDescription: Boolean = false): LaunchItem {
        val c = ContentManager.current(ctx, type) ?: return LaunchItem(label, name, "not installed", missing = true)
        val shown = if (useDescription) c.description.ifEmpty { name } else name
        return LaunchItem(label, shown, versionLabel(c.versionName))
    }

    fun resolve(ctx: Context, sc: Shortcut): LaunchInfo {
        val exe = ShortcutStore.resolveExe(ctx, sc.exe)
        val ctl = ControllerConfig.resolve(ctx, sc.id, exe)
        val dxvk = item(ctx, "DXVK", "DXVK", "DXVK")
        return LaunchInfo(
            rootfs = item(ctx, "imagefs", "Rootfs", "imagefs", useDescription = true),
            wine = item(ctx, "Proton", "Wine", "Proton Wine"),
            fex = item(ctx, "FEXCore", "FEX", "FEXCore"),
            dxvk = dxvk,
            dxvkEnabled = ContainerManager.isDxvkEnabled(ctx),
            controller = LaunchItem("Controller", ctl.name, "output: ${ctl.output}"),
            exePath = exe,
            exeExists = File(exe).exists(),
            arch = if (sc.arch != "auto") sc.arch else (PeInfo.arch(exe) ?: "unknown")
        )
    }
}
