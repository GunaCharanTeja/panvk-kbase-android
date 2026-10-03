package com.winlator.core;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;

/** panvk-launcher: file logger (logcat is not readable on the test device). */
public final class XLog {
    private static PrintWriter out;

    public static synchronized void open(File file) {
        try {
            if (out != null) out.close();
            out = new PrintWriter(new FileWriter(file, false), true);
        } catch (IOException e) {
            out = null;
        }
    }

    public static synchronized void log(String msg) {
        if (out != null) out.println(System.currentTimeMillis() % 1000000 + " " + msg);
    }

    public static synchronized void log(String msg, Throwable t) {
        StringWriter sw = new StringWriter();
        t.printStackTrace(new PrintWriter(sw));
        log(msg + "\n" + sw);
    }
}
