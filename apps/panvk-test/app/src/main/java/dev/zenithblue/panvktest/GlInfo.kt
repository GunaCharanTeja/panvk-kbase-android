package dev.zenithblue.panvktest

import android.opengl.EGL14
import android.opengl.EGLConfig
import android.opengl.GLES20
import org.json.JSONObject

data class GlInfo(val renderer: String?, val vendor: String?, val version: String?)

private val cachedGlInfo: GlInfo? by lazy(LazyThreadSafetyMode.SYNCHRONIZED) {
    try {
        readGlInfo()
    } catch (_: Exception) {
        null
    }
}

// Call from a background thread; failures are cached too.
fun queryGlInfo(): GlInfo? = cachedGlInfo

// Call from a background thread, just like queryGlInfo().
fun glInfoJson(): JSONObject {
    val info = queryGlInfo()
    return JSONObject().apply {
        put("renderer", info?.renderer ?: JSONObject.NULL)
        put("vendor", info?.vendor ?: JSONObject.NULL)
        put("version", info?.version ?: JSONObject.NULL)
    }
}

private fun readGlInfo(): GlInfo? {
    var display = EGL14.EGL_NO_DISPLAY
    var surface = EGL14.EGL_NO_SURFACE
    var context = EGL14.EGL_NO_CONTEXT
    var initialized = false
    try {
        display = EGL14.eglGetDisplay(EGL14.EGL_DEFAULT_DISPLAY)
        if (display == EGL14.EGL_NO_DISPLAY) return null
        val eglVersion = IntArray(2)
        if (!EGL14.eglInitialize(display, eglVersion, 0, eglVersion, 1)) return null
        initialized = true

        val configs = arrayOfNulls<EGLConfig>(1)
        val count = IntArray(1)
        val attributes = intArrayOf(
            EGL14.EGL_SURFACE_TYPE, EGL14.EGL_PBUFFER_BIT,
            EGL14.EGL_RENDERABLE_TYPE, EGL14.EGL_OPENGL_ES2_BIT,
            EGL14.EGL_RED_SIZE, 8,
            EGL14.EGL_GREEN_SIZE, 8,
            EGL14.EGL_BLUE_SIZE, 8,
            EGL14.EGL_ALPHA_SIZE, 8,
            EGL14.EGL_NONE
        )
        if (!EGL14.eglChooseConfig(display, attributes, 0, configs, 0, 1, count, 0) || count[0] == 0) return null
        val config = configs[0] ?: return null
        surface = EGL14.eglCreatePbufferSurface(
            display, config,
            intArrayOf(EGL14.EGL_WIDTH, 1, EGL14.EGL_HEIGHT, 1, EGL14.EGL_NONE), 0
        )
        if (surface == EGL14.EGL_NO_SURFACE) return null
        context = EGL14.eglCreateContext(
            display, config, EGL14.EGL_NO_CONTEXT,
            intArrayOf(EGL14.EGL_CONTEXT_CLIENT_VERSION, 2, EGL14.EGL_NONE), 0
        )
        if (context == EGL14.EGL_NO_CONTEXT) return null
        if (!EGL14.eglMakeCurrent(display, surface, surface, context)) return null

        fun glString(name: Int): String? = GLES20.glGetString(name)?.trim()?.takeIf { it.isNotEmpty() }
        val info = GlInfo(
            renderer = glString(GLES20.GL_RENDERER),
            vendor = glString(GLES20.GL_VENDOR),
            version = glString(GLES20.GL_VERSION)
        )
        return if (info.renderer == null && info.vendor == null && info.version == null) null else info
    } finally {
        if (display != EGL14.EGL_NO_DISPLAY) {
            runCatching { EGL14.eglMakeCurrent(display, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_SURFACE, EGL14.EGL_NO_CONTEXT) }
            if (surface != EGL14.EGL_NO_SURFACE) runCatching { EGL14.eglDestroySurface(display, surface) }
            if (context != EGL14.EGL_NO_CONTEXT) runCatching { EGL14.eglDestroyContext(display, context) }
            if (initialized) runCatching { EGL14.eglTerminate(display) }
        }
        runCatching { EGL14.eglReleaseThread() }
    }
}
