package org.baron.krita;

import java.nio.charset.StandardCharsets;

public final class AnrReportTest {
    public static void main(String[] args) {
        String trace = "PRIVATE_HEADER\n"
            + "\"main\" prio=5 tid=1 Native\n"
            + "  | group=\"main\" sCount=1\n"
            + "  native: #00 pc 000123 /system/lib64/libc.so (futex_wait+4)\n"
            + "  at org.qtproject.qt5.android.QtInputConnection.setSelection(Native method)\n"
            + "  - waiting to lock <0x0012> (a java.lang.Object)\n"
            + "PRIVATE_PROMPT_AND_TOKEN\n\n"
            + "\"qtMainLoopThread\" prio=5 tid=17 Runnable\n"
            + "  at org.qtproject.qt5.android.QtNative.startApplication(QtNative.java:123)\n\n";
        String result = AnrReport.format(trace.getBytes(StandardCharsets.UTF_8));
        if (!result.contains("Thread: main tid=1 Native") || !result.contains("futex_wait+4")
            || !result.contains("setSelection") || !result.contains("qtMainLoopThread")
            || !result.contains("waiting to lock")) throw new AssertionError(result);
        if (result.contains("PRIVATE") || result.contains("sCount")) throw new AssertionError("Unexpected private fields");
        if (!AnrReport.format(new byte[0]).contains("No supported")) throw new AssertionError("Empty trace");
        try {
            AnrReport.format(new byte[4 * 1024 * 1024 + 1]);
            throw new AssertionError("Accepted oversized trace");
        } catch (IllegalArgumentException expected) { }
        System.out.println("ANR parser: Java/native frames, thread states, locks, privacy, empty and oversized traces passed");
    }
}
