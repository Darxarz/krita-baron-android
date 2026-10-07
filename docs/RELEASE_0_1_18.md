# Android 0.1.18 — clipboard paste protection

The user reports an Android crash when pasting into the prompt. Ordinary clipboard
paste and IME text commits did not reproduce the crash in desktop Qt/ASAN tests.
There is no attached Samsung device or crash stack, so the exact device failure
remains unconfirmed. This release hardens the Android-specific paths for testing.

- Embedded completion no longer attaches QCompleter's keyboard event filter or
  creates its separate popup. Its model still supplies the embedded suggestion list.
- Android's native edit popup sends a paste shortcut from a synchronous IME batch.
  The editor consumes this shortcut and schedules clipboard access after returning
  from that event. A small Java helper reads only ClipData's text on the Android UI
  thread; null/empty clips and runtime exceptions produce no insertion. HTML and
  URI clipboard formats are not fetched by this shortcut path.
- The Qt context menu's paste action uses the same queued path. Qt's standard menu
  creation still checks availability through its clipboard implementation.
- Text is returned to the Qt thread for plain-text insertion. The editor must still
  exist, retain focus, and have the same document revision and cursor selection.
  Focus changes, destruction and concurrent edits cannot paste into another field.
- MIME paste/drop also copies text before inserting. Completion is hidden and its
  timer stopped during paste and multiword/long IME commits; normal subsequent
  typing resumes suggestions. Short single-word IME commits retain completion.
- Suggestion coordinates use global-to-host mapping and the list follows a
  reparented editor, avoiding non-ancestor QWidget coordinate mapping.

Regression coverage includes Cyrillic, emoji, multiline text, LoRA syntax,
selection replacement, undo/redo, clipboard text taking precedence over HTML,
IME bulk commits, deferred paste cancellation and completion after reparenting.
Full Windows and Linux ASAN runs pass 84 tests with 2 optional integration tests
skipped. APK compilation, package/signature/source and public-download verification
are recorded separately in artifacts. A desktop pass is not Samsung acceptance.

No generation workflow, selection-feathering preference, server or Python plugin
was changed for this release.
