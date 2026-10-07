# Android 0.1.21 — obtain the actual device crash location

The Samsung user confirmed that 0.1.20 still crashes when pasting their long
prompt. The text is already present after restarting. This does not prove that
clipboard reading failed: text may have been inserted and saved before another
operation crashed, or restored from the global settings. Desktop/ASAN tests
have not reproduced this crash. This release does not claim to fix it.

Settings → Plugin → Crash diagnostics reads Android's retained exit history.
On Android 12+ the newest retained native crash supplies a tombstone through
ApplicationExitInfo.getTraceInputStream, including an already recorded 0.1.20
crash. A computer, USB debugging, account token, or another deliberate crash
is not required when Android still retains that trace. No report is uploaded.
Copy report and Save report let the user choose how to provide it.

The reader reports exit reason (native/Java/ANR/low memory/etc.), memory counters,
signal, fault address, and up to 64 frames from the crashed thread. Relative PCs,
library names and build IDs permit symbolization against the matching binaries.
It does not export memory dumps, Android log buffers, file descriptors, command
lines or abort messages. The protobuf reader rejects truncated/oversized data.
Android API availability and missing/expired traces are handled explicitly.

Local bounded stage logs record clipboard read/insertion, syntax highlighting,
IME queries, first repaint for each revision, and settings/document/history
saving. Only stage names and numeric sizes/positions are recorded, never prompt
contents, clipboard contents, image data or credentials. The previous session's
last stages remain available after a restart.

The report runs on a worker thread, and closing its read-only dialog before
completion does not delete a running QThread. Copy/save become available once
the system report has loaded. Java parser tests cover stack decoding, selecting
the crashed thread, unsigned addresses, omitted private fields and malformed
messages. The Qt tests cover report lifecycle and numeric stage logging.

References:
- https://developer.android.com/ndk/guides/debug
- https://android.googlesource.com/platform/system/core/+/refs/heads/main/debuggerd/proto/tombstone.proto

Physical Samsung crash root cause and final fix remain pending the device report.
