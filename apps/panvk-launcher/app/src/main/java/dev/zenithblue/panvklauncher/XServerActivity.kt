package dev.zenithblue.panvklauncher

import android.app.Activity
import android.os.Bundle
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.FrameLayout
import com.winlator.widget.TouchpadView
import com.winlator.widget.XServerView

/**
 * Surface + input for the built-in X server. Games render into the X server's
 * windows; XServerView composites them with GLES, TouchpadView turns touches
 * into pointer events, keyboard goes straight to the X keyboard.
 */
class XServerActivity : Activity() {
    private var touchpad: TouchpadView? = null
    private var xView: XServerView? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val xs = BuiltinXServer.xServer
        if (xs == null) {
            finish()
            return
        }
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val root = FrameLayout(this)
        val view = XServerView(this, xs)
        xs.setRenderer(view.renderer)
        val pad = TouchpadView(this, xs)
        val overlay = GamepadOverlayView(this)
        // Touch overlay collapses to a SHOW button when a hardware pad is attached; four-finger tap toggles.
        overlay.hidden = GamepadBridge.physicalConnected()
        pad.setFourFingersTapCallback { overlay.hidden = !overlay.hidden }
        if (android.os.Build.VERSION.SDK_INT >= 28) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        root.addView(view)
        root.addView(pad)
        root.addView(overlay, FrameLayout.LayoutParams(-1, -1))
        setContentView(root)
        xView = view
        touchpad = pad
        BuiltinXServer.attach(this)
        hideSystemBars()
    }

    private fun hideSystemBars() {
        @Suppress("DEPRECATION")
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
            View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
            View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or View.SYSTEM_UI_FLAG_FULLSCREEN or
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) hideSystemBars()
    }

    override fun onResume() {
        super.onResume()
        xView?.onResume()
    }

    override fun onPause() {
        xView?.onPause()
        super.onPause()
    }

    override fun onDestroy() {
        BuiltinXServer.detach(this)
        super.onDestroy()
    }

    // Back leaves the display. The game keeps running; Open display in the launcher returns.
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val xs = BuiltinXServer.xServer ?: return super.dispatchKeyEvent(event)
        if (event.keyCode == KeyEvent.KEYCODE_BACK) return super.dispatchKeyEvent(event)
        if (GamepadBridge.onKeyEvent(event)) return true
        return xs.keyboard.onKeyEvent(event) || super.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean =
        GamepadBridge.onMotionEvent(event) || touchpad?.onExternalMouseEvent(event) == true ||
            super.dispatchGenericMotionEvent(event)
}
