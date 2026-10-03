package com.winlator.xserver.extensions;

import static com.winlator.xserver.XClientRequestHandler.RESPONSE_CODE_SUCCESS;

import com.winlator.xconnector.XInputStream;
import com.winlator.xconnector.XOutputStream;
import com.winlator.xconnector.XStreamLock;
import com.winlator.xserver.XClient;
import com.winlator.xserver.errors.BadImplementation;
import com.winlator.xserver.errors.XRequestError;
import com.winlator.xserver.events.Event;

import java.io.IOException;
import java.util.concurrent.ConcurrentHashMap;

/**
 * panvk-launcher: the smallest XInput 2.2 Wine needs for raw mouse input. Wine's winex11 only produces WM_INPUT
 * (raw input, what Unity and most 3D games read for mouse look) from XI_RawMotion events; core MotionNotify is sent
 * with SEND_HWMSG_NO_RAW. Implements GetExtensionVersion, XIGetClientPointer, XISelectEvents, XIQueryVersion,
 * XIQueryDevice and emits XI_RawMotion with relative X/Y valuators. One master pointer (2) + master keyboard (3).
 */
public class XInput2Extension implements Extension {
    public static final byte MAJOR_OPCODE = -105;
    private static final short POINTER_ID = 2, KEYBOARD_ID = 3;
    private static final int XI_RAW_MOTION = 17;

    /** Clients that selected XI_RawMotion (on the root window). */
    private final ConcurrentHashMap<XClient, Boolean> rawMotionClients = new ConcurrentHashMap<>();

    @Override public String getName() { return "XInputExtension"; }
    @Override public byte getMajorOpcode() { return MAJOR_OPCODE; }
    @Override public byte getFirstErrorId() { return (byte)150; }
    // XI1 events of libXi occupy first_event..first_event+16; keep them clear of core events and MIT-SHM (64).
    @Override public byte getFirstEventId() { return 70; }

    @Override
    public void handleRequest(XClient client, XInputStream in, XOutputStream out) throws IOException, XRequestError {
        int minor = client.getRequestData() & 0xff;
        switch (minor) {
            case 1: { // X_GetExtensionVersion
                client.skipRequest();
                try (XStreamLock lock = out.lock()) {
                    out.writeByte(RESPONSE_CODE_SUCCESS); out.writeByte((byte)1);
                    out.writeShort(client.getSequenceNumber()); out.writeInt(0);
                    out.writeShort((short)2); out.writeShort((short)2);
                    out.writeByte((byte)1); // present
                    out.writePad(19);
                }
                break;
            }
            case 45: { // X_XIGetClientPointer
                client.skipRequest();
                try (XStreamLock lock = out.lock()) {
                    out.writeByte(RESPONSE_CODE_SUCCESS); out.writeByte((byte)45);
                    out.writeShort(client.getSequenceNumber()); out.writeInt(0);
                    out.writeByte((byte)1); out.writeByte((byte)0); out.writeShort(POINTER_ID);
                    out.writePad(20);
                }
                break;
            }
            case 46: { // X_XISelectEvents
                int window = in.readInt();
                int numMasks = in.readUnsignedShort();
                in.skip(2);
                boolean raw = false, any = false;
                for (int i = 0; i < numMasks; i++) {
                    in.skip(2); // deviceid (we have one pointer)
                    int len = in.readUnsignedShort();
                    for (int w = 0; w < len; w++) {
                        int bits = in.readInt();
                        if (w == 0 && (bits & (1 << XI_RAW_MOTION)) != 0) raw = true;
                        if (bits != 0) any = true;
                    }
                }
                client.skipRequest();
                if (window == client.xServer.windowManager.rootWindow.id) {
                    if (raw) rawMotionClients.put(client, Boolean.TRUE);
                    else rawMotionClients.remove(client);
                }
                break;
            }
            case 47: { // X_XIQueryVersion
                client.skipRequest();
                try (XStreamLock lock = out.lock()) {
                    out.writeByte(RESPONSE_CODE_SUCCESS); out.writeByte((byte)47);
                    out.writeShort(client.getSequenceNumber()); out.writeInt(0);
                    out.writeShort((short)2); out.writeShort((short)2);
                    out.writePad(20);
                }
                break;
            }
            case 48: { // X_XIQueryDevice
                int deviceId = in.readUnsignedShort();
                client.skipRequest();
                boolean ptr = deviceId == 0 || deviceId == 1 || deviceId == POINTER_ID;
                boolean kbd = deviceId == 0 || deviceId == 1 || deviceId == KEYBOARD_ID;
                if (!ptr && !kbd) throw new BadImplementation(); // ponytail: should be BadDevice; no slave devices exist
                // device header 12 + name 4 ("ptr"/"kbd" padded); the pointer adds two 44-byte valuator classes
                int bytes = (ptr ? 12 + 4 + 88 : 0) + (kbd ? 12 + 4 : 0);
                try (XStreamLock lock = out.lock()) {
                    out.writeByte(RESPONSE_CODE_SUCCESS); out.writeByte((byte)48);
                    out.writeShort(client.getSequenceNumber()); out.writeInt(bytes / 4);
                    out.writeShort((short)((ptr ? 1 : 0) + (kbd ? 1 : 0)));
                    out.writePad(22);
                    if (ptr) {
                        writeDeviceHeader(out, POINTER_ID, 1 /* XIMasterPointer */, KEYBOARD_ID, 2, "ptr");
                        for (short axis = 0; axis < 2; axis++) {
                            out.writeShort((short)2); out.writeShort((short)11); // XIValuatorClass, 44 bytes
                            out.writeShort(POINTER_ID); out.writeShort(axis);
                            out.writeInt(0); // label None
                            out.writeInt(-1); out.writeInt(0); // min -1.0
                            out.writeInt(-1); out.writeInt(0); // max -1.0 (max <= min: Wine uses scale 1)
                            out.writeInt(0); out.writeInt(0); // value
                            out.writeInt(0); // resolution
                            out.writeByte((byte)0); // XIModeRelative
                            out.writePad(3);
                        }
                    }
                    if (kbd) writeDeviceHeader(out, KEYBOARD_ID, 2 /* XIMasterKeyboard */, POINTER_ID, 0, "kbd");
                }
                break;
            }
            default:
                throw new BadImplementation();
        }
    }

    private static void writeDeviceHeader(XOutputStream out, short id, int use, short attachment, int numClasses, String name) {
        out.writeShort(id); out.writeShort((short)use); out.writeShort(attachment);
        out.writeShort((short)numClasses); out.writeShort((short)name.length());
        out.writeByte((byte)1); out.writeByte((byte)0); // enabled
        out.writeString8(name); // 3 chars, padded to 4
    }

    /** Relative pointer motion from an input device (not from client warps). */
    public void sendRawMotion(int dx, int dy) {
        if (rawMotionClients.isEmpty() || (dx == 0 && dy == 0)) return;
        RawMotion event = new RawMotion(dx, dy, (int)android.os.SystemClock.uptimeMillis());
        for (XClient c : rawMotionClients.keySet()) {
            if (!c.sendEventChecked(event)) rawMotionClients.remove(c);
        }
    }

    private static class RawMotion extends Event {
        private final int dx, dy, time;

        RawMotion(int dx, int dy, int time) {
            super(35);
            this.dx = dx; this.dy = dy; this.time = time;
        }

        @Override
        public void send(short sequenceNumber, XOutputStream out) throws IOException {
            try (XStreamLock lock = out.lock()) {
                out.writeByte(code); out.writeByte(MAJOR_OPCODE);
                out.writeShort(sequenceNumber);
                out.writeInt(9); // 36 extra bytes: mask(4) + values(16) + raw values(16)
                out.writeShort((short)XI_RAW_MOTION); out.writeShort(POINTER_ID);
                out.writeInt(time);
                out.writeInt(0); // detail
                out.writeShort(POINTER_ID); out.writeShort((short)1); // sourceid, valuators_len (4-byte units)
                out.writeInt(0); out.writeInt(0); // flags, pad
                out.writeInt(0x3); // valuators 0 (x) and 1 (y)
                out.writeInt(dx); out.writeInt(0); out.writeInt(dy); out.writeInt(0); // accelerated values (FP3232)
                out.writeInt(dx); out.writeInt(0); out.writeInt(dy); out.writeInt(0); // raw values
            }
        }
    }
}
