# Android 0.1.19 — copying prompts as plain text

The user clarified that the crash concerns a long prompt copied from Krita,
while other text pastes normally. The exact prompt is retained only as a local
fixture under the ignored build directory, including U+200B before thick_eyebrows
and both LoRA tags. It is excluded from public sources. BARON_SELF_COPY_FIXTURE
selects it for local verification; a neutral structural fixture ships with tests.
No prompt syntax or hidden characters are stripped by the fix.

Desktop and Linux ASAN tests completed ten self-copy/paste cycles of this exact
prompt without a crash, including destruction of the source editor before paste.
They do not reproduce the physical Samsung failure.

Inspection confirmed a difference in the copy path: Qt's default plain-text editor
still creates a lazy QTextEditMimeData with text/plain, text/html and OpenDocument
formats. Retrieving its text calls setup(), which also serializes HTML and ODF.
For Android/embedded prompt fields, createMimeDataFromSelection now returns a
fully materialized QMimeData containing only text/plain. Both copy and cut use this
override. The ordinary desktop popup editor retains the original Qt behavior.

The regression first failed against 0.1.18 because its copy object advertised three
formats, rather than plain text only. It now checks the owned plain-text snapshot
and exact user text, then performs the same repeated self-copy/paste and undo/redo.
Additional coverage checks multiline copy/cut and copying while suggestions are
visible. This tests removal of the formatted clipboard path; it does not establish
that this was the exact crash cause on Samsung.

The clipboard, deferred paste, IME and focus protections from 0.1.18 are preserved.
No generation workflow or selection preferences changed. Physical-device crash
acceptance remains pending; build/test/package/publication evidence is recorded
in the release artifacts.
