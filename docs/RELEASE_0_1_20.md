# Android 0.1.20 — release version and history copy between documents

The native interface displayed 0.1.16 because BaronUpdates::version() contained a
hardcoded string that was not changed for the newer APKs. Android's actual
versionCode continued to advance and update comparison used PackageManager.
This explains the stale label; it is not evidence that package installation
failed. Both CMake builds now read android/version.json (copied into the Krita
overlay as version.json), and the Android interface reads the installed package's
versionName. A metadata consistency test guards against another stale label.
Failure to read the installed version code now reports an update failure rather
than incorrectly claiming that no update exists.
Manual checks also work after downloading a package, and feed requests bypass
the network cache. Previously a ready download made the check button a no-op.

The user clarified the clipboard path: restore Windows Python history from a
.kra, choose Copy Prompt, switch to a new document, then paste. The 0.1.19
selection-copy override did not cover the history action, which wrote directly
to Qt's Android clipboard bridge. History Copy Prompt, Copy Evaluated Prompt,
copying generation settings, and touch-editor copy/cut now share a plain-text
clipboard writer. On Android it uses ClipData.newPlainText on the Android UI
thread, matching the existing direct Android clipboard reader. The touch editor
also constructs its editing menu without synchronous Qt MIME clipboard queries.
Other applications' clipboard text remains supported.

Prompt replacement and document changes reset completion and preedit state and
invalidate pending pastes. Clipboard requests capture only cursor coordinates
and a generation counter; QTextCursor objects are no longer carried across the
Android and Qt threads. Late clipboard callbacks cannot edit a different project
even if its text revision and cursor position happen to match the previous one.

The regression fixture is exported by the real Python plugin's JobParams and
history serializer. Tests exercise the native history QAction, copying both
prompts, five new-document paste cycles, undo/redo, and saving each document's
state. The user's exact prompt is used only through ignored local fixtures;
the public fixture is a neutral structural equivalent with LoRA tags and U+200B.
Tests also cover queued paste cancellation and update checks for both the
current package and a newer package.

Windows/Linux offscreen and ASAN checks do not reproduce a physical Samsung
clipboard crash. These changes remove unsafe paths identified during review;
the crash's precise device-side cause remains unconfirmed without device logs.
Real Samsung acceptance is still required.
