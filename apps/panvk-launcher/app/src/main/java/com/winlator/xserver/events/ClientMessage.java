package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

/** panvk-launcher: format-32 ClientMessage (33), e.g. WM_PROTOCOLS / WM_TAKE_FOCUS from the "window manager". */
public class ClientMessage extends Event {
    private final Window window;
    private final int type;
    private final int[] data;

    public ClientMessage(Window window, int type, int... data) {
        super(33);
        this.window = window;
        this.type = type;
        this.data = data;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)32);
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window.id);
            outputStream.writeInt(type);
            for (int i = 0; i < 5; i++) outputStream.writeInt(i < data.length ? data[i] : 0);
        }
    }
}
