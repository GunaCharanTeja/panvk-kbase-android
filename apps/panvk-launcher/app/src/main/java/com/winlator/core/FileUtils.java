package com.winlator.core;

import java.io.File;
import java.nio.file.Files;

/**
 * Trimmed from Winlator's FileUtils (LGPL-2.1): only the helpers the built-in X
 * server's UnixSocketConfig needs.
 */
public abstract class FileUtils {
    public static boolean isSymlink(File file) {
        return Files.isSymbolicLink(file.toPath());
    }

    public static boolean delete(File targetFile) {
        if (targetFile == null) return false;
        if (targetFile.isDirectory()) {
            if (!isSymlink(targetFile)) if (!clear(targetFile)) return false;
        }
        return targetFile.delete();
    }

    public static boolean clear(File targetFile) {
        if (targetFile == null) return false;
        if (targetFile.isDirectory()) {
            File[] files = targetFile.listFiles();
            if (files != null) {
                for (File file : files) {
                    if (!delete(file)) return false;
                }
            }
        }
        return true;
    }

    public static String getDirname(String path) {
        if (path == null) return "";
        path = StringUtils.removeEndSlash(path);
        int index = Math.max(path.lastIndexOf('/'), path.lastIndexOf('\\'));
        return path.substring(0, index);
    }
}
