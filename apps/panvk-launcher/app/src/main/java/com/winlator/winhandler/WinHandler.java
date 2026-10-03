package com.winlator.winhandler;

/**
 * panvk-launcher stub. Upstream Winlator's WinHandler talks to a helper exe inside
 * Wine (relative mouse, bring-to-front, process list). We ship none of that, so
 * these calls are no-ops. Relative mouse movement is therefore left disabled.
 */
public class WinHandler {
    public void mouseEvent(int flags, int dx, int dy, int wheelDelta) {}

    public void bringToFront(String className, long handle) {}
}
