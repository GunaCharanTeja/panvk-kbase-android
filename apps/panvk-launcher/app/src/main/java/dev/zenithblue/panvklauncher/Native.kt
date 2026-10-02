package dev.zenithblue.panvklauncher

object Native {
    init {
        System.loadLibrary("panvklauncher")
    }

    @JvmStatic
    external fun probe(path: String): String
}
