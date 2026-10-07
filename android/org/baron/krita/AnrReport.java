// SPDX-License-Identifier: GPL-3.0-or-later
package org.baron.krita;

import java.nio.charset.StandardCharsets;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public final class AnrReport {
    private static final Pattern THREAD = Pattern.compile("^\"([^\"\\r\\n]{1,128})\".*\\btid=(\\d+).*? (Runnable|Native|Waiting|TimedWaiting|Blocked|Suspended).*$");
    private static final Pattern JAVA_FRAME = Pattern.compile("^at [\\w.$<>]+\\([^()\\r\\n]{0,256}\\)$");
    private static final Pattern NATIVE_FRAME = Pattern.compile("^(?:native: )?#\\d+ pc [0-9a-fA-F]+ [^\\r\\n]{1,512}$");
    private static final Pattern LOCK = Pattern.compile("^- (?:waiting on|waiting to lock|locked) <0x[0-9a-fA-F]+>.*$");

    public static String format(byte[] data) {
        if (data.length > 4 * 1024 * 1024) throw new IllegalArgumentException("ANR trace too large");
        StringBuilder output = new StringBuilder("ANR thread stacks:\n");
        boolean inThread = false;
        int threads = 0, frames = 0;
        for (String line : new String(data, StandardCharsets.UTF_8).split("\n")) {
            String text = line.trim();
            Matcher thread = THREAD.matcher(text);
            if (thread.matches()) {
                if (++threads > 128) break;
                inThread = true;
                frames = 0;
                output.append("\nThread: ").append(thread.group(1)).append(" tid=")
                    .append(thread.group(2)).append(' ').append(thread.group(3)).append('\n');
            } else if (text.isEmpty()) {
                inThread = false;
            } else if (inThread && frames < 64 && (JAVA_FRAME.matcher(text).matches()
                || NATIVE_FRAME.matcher(text).matches() || LOCK.matcher(text).matches())) {
                output.append(text).append('\n');
                ++frames;
            }
            if (output.length() > 128 * 1024) break;
        }
        if (threads == 0) output.append("No supported thread stacks in Android trace.\n");
        return output.toString();
    }
    private AnrReport() { }
}
