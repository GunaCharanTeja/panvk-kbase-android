package dev.zenithblue.panvktest

object Native {
    init {
        System.loadLibrary("panvktest")
    }

    @JvmStatic
    external fun run(
        libPath: String,
        args: Array<String>,
        env: Array<String>,
        logPath: String,
        timeoutMs: Int
    ): String

    /** In-process (binder/ANativeWindow do not survive fork). Blocks; returns "exit:0" or "exit:1". */
    @JvmStatic
    external fun swapchainTest(driverPath: String, surface: android.view.Surface, logPath: String): String

    /** "<phase> <msInPhase> <done 0|1>" for the Kotlin watchdog. */
    @JvmStatic
    external fun swapchainPhase(): String
}
