package com.winlator.xserver;

import androidx.collection.ArrayMap;

import com.winlator.xserver.events.ClientMessage;

import java.util.Map;

public abstract class DesktopHelper {
    public static void attachTo(final XServer xServer) {
        setupXResources(xServer);

        xServer.pointer.addOnPointerMotionListener(new Pointer.OnPointerMotionListener() {
            @Override
            public void onPointerButtonPress(Pointer.Button button) {
                updateFocusedWindow(xServer);
            }
        });

        xServer.windowManager.addOnWindowModificationListener(new WindowManager.OnWindowModificationListener() {
            @Override
            public void onMapWindow(Window window) {
                if (isTopLevel(xServer, window)) setWMState(window, 1); // NormalState
                setFocusedWindow(xServer, window);
            }

            @Override
            public void onUnmapWindow(Window window) {
                if (isTopLevel(xServer, window)) setWMState(window, 0); // WithdrawnState
            }
        });
    }

    private static boolean isTopLevel(XServer xServer, Window window) {
        return window.getParent() == xServer.windowManager.rootWindow && !window.attributes.isOverrideRedirect();
    }

    // panvk: a window manager sets WM_STATE when it maps/withdraws a top-level. Wine (10+) waits for that change and
    // ignores FocusIn / WM_TAKE_FOCUS "during WM_STATE change" until it sees it, so without a WM it never settles.
    private static void setWMState(Window window, int state) {
        int atom = Atom.internAtom("WM_STATE");
        byte[] data = java.nio.ByteBuffer.allocate(8).order(java.nio.ByteOrder.LITTLE_ENDIAN).putInt(state).putInt(0).array();
        window.modifyProperty(atom, atom, Property.Format.INT_ARRAY, Property.Mode.REPLACE, data);
    }

    private static void updateFocusedWindow(XServer xServer) {
        try (XLock lock = xServer.lock(XServer.Lockable.WINDOW_MANAGER, XServer.Lockable.INPUT_DEVICE)) {
            Window focusedWindow = xServer.windowManager.getFocusedWindow();
            Window child = xServer.windowManager.findPointWindow(xServer.pointer.getClampedX(), xServer.pointer.getClampedY());
            // panvk: click-to-focus works on the top-level (the child of root), not on whatever leaf is under the pointer.
            Window top = child;
            while (top != null && top.getParent() != null && top.getParent() != xServer.windowManager.rootWindow) top = top.getParent();
            if (top == null && focusedWindow != xServer.windowManager.rootWindow) {
                xServer.windowManager.setFocus(xServer.windowManager.rootWindow, WindowManager.FocusRevertTo.POINTER_ROOT);
            }
            else if (top != null && top != focusedWindow && !top.isAncestorOf(focusedWindow)) {
                setFocusedWindow(xServer, top);
            }
        }
    }

    // panvk: Winlator required isApplicationWindow() (WM_NAME as STRING + WM_HINTS group == self), which Wine's
    // windows fail (UTF-8 names), so nothing ever got focus. Any mapped, visible, non-override-redirect top-level
    // window is what a window manager would focus.
    private static boolean isFocusable(XServer xServer, Window window) {
        return window.getParent() == xServer.windowManager.rootWindow && window.attributes.isMapped() &&
               !window.attributes.isOverrideRedirect() && window.getWidth() > 1 && window.getHeight() > 1;
    }

    private static void setFocusedWindow(XServer xServer, Window window) {
        if (window.isApplicationWindow() || isFocusable(xServer, window)) {
            boolean parentIsRoot = window.getParent() == xServer.windowManager.rootWindow;
            xServer.windowManager.setFocus(window, parentIsRoot ? WindowManager.FocusRevertTo.POINTER_ROOT : WindowManager.FocusRevertTo.PARENT);
            xServer.getWinHandler().bringToFront(window.getClassName(), window.getHandle());
            takeFocus(window);
        }
    }

    // panvk: act as the window manager. Wine (UseTakeFocus, the default) only activates a window, i.e. makes it the
    // Win32 foreground window that gets keyboard and raw mouse input, when the WM sends WM_TAKE_FOCUS. Upstream
    // Winlator did this through its winhandler.exe helper (bringToFront), which we do not ship.
    private static void takeFocus(Window window) {
        Property protocols = window.getProperty(Atom.getId("WM_PROTOCOLS"));
        int takeFocus = Atom.getId("WM_TAKE_FOCUS");
        if (protocols == null || takeFocus <= 0) return;
        for (int i = 0; i < protocols.data.capacity() / 4; i++) {
            if (protocols.getInt(i) == takeFocus) {
                com.winlator.core.XLog.log("WM_TAKE_FOCUS -> " + window.id + " '" + window.getName() + "'");
                window.originClient.sendEvent(new ClientMessage(window, protocols.name, takeFocus, (int)android.os.SystemClock.uptimeMillis()));
                return;
            }
        }
    }

    private static void setupXResources(XServer xServer) {
        int atom = Atom.getId("RESOURCE_MANAGER");
        int type = Atom.getId("STRING");

        ArrayMap<String, String> values = new ArrayMap<>();
        values.put("size", "20");
        values.put("theme", "dmz");
        values.put("theme_core", "true");

        StringBuilder sb = new StringBuilder();
        for (Map.Entry<String, String> entry : values.entrySet()) {
            sb.append("Xcursor")
              .append('.')
              .append(entry.getKey())
              .append(':')
              .append('\t')
              .append(entry.getValue())
              .append('\n');
        }

        byte[] data = sb.toString().getBytes(XServer.LATIN1_CHARSET);
        xServer.windowManager.rootWindow.modifyProperty(atom, type, Property.Format.BYTE_ARRAY, Property.Mode.APPEND, data);
    }
}