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
}
