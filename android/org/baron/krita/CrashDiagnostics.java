// SPDX-License-Identifier: GPL-3.0-or-later
package org.baron.krita;

import android.app.ActivityManager;
import android.app.ApplicationExitInfo;
import android.content.Context;
import android.os.Build;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.util.List;

public final class CrashDiagnostics {
    private static String reason(int value) {
        switch (value) {
            case ApplicationExitInfo.REASON_CRASH: return "Java crash";
            case ApplicationExitInfo.REASON_CRASH_NATIVE: return "Native crash";
            case ApplicationExitInfo.REASON_ANR: return "Application not responding";
            case ApplicationExitInfo.REASON_LOW_MEMORY: return "Android killed the app: low memory";
            case ApplicationExitInfo.REASON_USER_REQUESTED: return "User/system stopped the app";
            case ApplicationExitInfo.REASON_EXIT_SELF: return "App exited itself";
            case ApplicationExitInfo.REASON_SIGNALED: return "Process received a signal";
            default: return "Other exit reason " + value;
        }
    }
    public static String report(Context context) {
        StringBuilder output = new StringBuilder("Krita Baron Android crash report\n");
        output.append("Installed: ").append(Updates.installedName(context))
            .append(" code=").append(Updates.installedCode(context)).append('\n');
        output.append("Device: ").append(Build.MANUFACTURER).append(' ').append(Build.MODEL)
            .append(" Android ").append(Build.VERSION.RELEASE).append(" API ").append(Build.VERSION.SDK_INT).append('\n');
        if (Build.VERSION.SDK_INT < 30) return output.append("System exit history requires Android 11.\n").toString();
        try {
            ActivityManager manager = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (manager == null) return output.append("Exit history unavailable.\n").toString();
            List<ApplicationExitInfo> exits = manager.getHistoricalProcessExitReasons(context.getPackageName(), 0, 8);
            if (exits.isEmpty()) output.append("No previous process exits recorded by Android.\n");
            boolean nativeTraceRead = false, anrTraceRead = false;
            for (ApplicationExitInfo exit : exits) {
                output.append("\nExit timestamp=").append(exit.getTimestamp()).append(" reason=")
                    .append(reason(exit.getReason())).append(" status=").append(exit.getStatus())
                    .append(" PSS(kB)=").append(exit.getPss()).append(" RSS(kB)=").append(exit.getRss()).append('\n');
                boolean nativeCrash = exit.getReason() == ApplicationExitInfo.REASON_CRASH_NATIVE && Build.VERSION.SDK_INT >= 31;
                // A recovered ANR trace can be attached to a later exit for another reason.
                boolean anr = !nativeCrash;
                if (anr ? anrTraceRead : nativeTraceRead) continue;
                try (InputStream input = exit.getTraceInputStream()) {
                    if (input == null) {
                        if (nativeCrash || exit.getReason() == ApplicationExitInfo.REASON_ANR)
                            output.append("Android did not retain a ").append(anr ? "hang" : "native").append(" stack trace.\n");
                        continue;
                    }
                    ByteArrayOutputStream data = new ByteArrayOutputStream();
                    byte[] buffer = new byte[16384];
                    int count;
                    while ((count = input.read(buffer)) != -1) {
                        if (data.size() + count > 4 * 1024 * 1024) throw new IllegalArgumentException("Trace exceeds 4 MiB limit");
                        data.write(buffer, 0, count);
                    }
                    String trace = anr ? AnrReport.format(data.toByteArray()) : TombstoneReport.format(data.toByteArray());
                    output.append(trace);
                    if (anr) anrTraceRead = trace.contains("Thread: "); else nativeTraceRead = true;
                } catch (Exception error) {
                    output.append("Trace unavailable: ").append(error.getClass().getSimpleName()).append('\n');
                }
            }
        } catch (RuntimeException error) {
            output.append("System exit history unavailable: ").append(error.getClass().getSimpleName()).append('\n');
        }
        return output.toString();
    }
    private CrashDiagnostics() { }
}
