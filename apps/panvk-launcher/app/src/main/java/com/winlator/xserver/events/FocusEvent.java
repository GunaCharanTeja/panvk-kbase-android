package com.winlator.xserver.events;

import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.Window;

import java.io.IOException;

/** panvk-launcher: core FocusIn (9) / FocusOut (10), detail Nonlinear, mode Normal. */
public class FocusEvent extends Event {
    private final Window window;

    public FocusEvent(boolean in, Window window) {
        super(in ? 9 : 10);
        this.window = window;
    }

    @Override
    public void send(short sequenceNumber, XOutputStream outputStream) throws IOException {
        try (XStreamLock lock = outputStream.lock()) {
            outputStream.writeByte(code);
            outputStream.writeByte((byte)3); // NotifyNonlinear
            outputStream.writeShort(sequenceNumber);
            outputStream.writeInt(window.id);
            outputStream.writeByte((byte)0); // NotifyNormal
            outputStream.writePad(23);
        }
    }
}
