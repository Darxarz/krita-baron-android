# Android 0.1.22 — Samsung prompt IME selection crash

The user's 0.1.21 diagnostic report retained a native crash from 0.1.20.
The native docker build ID is `3d7f6950800489526cead22cd16f151014867db5`,
matching the saved unstripped 0.1.20 module. Frame `0x571d4c` symbolizes to
`PromptEditor::inputMethodEvent`. The Android Qt Widgets binary symbolizes
the preceding frames to `QPlainTextEditPrivate::ensureVisible`,
`ensureCursorVisible`, `QPlainTextEditControl::ensureCursorVisible`, and
`QWidgetTextControlPrivate::inputMethodEvent`. Disassembly at `0x474b0c`
identifies the cursor visibility call in the **Selection attribute** branch.
The failing operation is `QTextLine::naturalTextRect()` on an invalid line.

A standalone QPlainTextEdit reproducer also crashes in that function on
Windows Qt 5.15.2 (access violation) and Linux Qt 5.15.13 (ASAN null read).
It uses a neutral long single-line prompt, a small wrapped editor, an active
preedit, and a commit with a Selection attribute. The original private user
prompt is not included in the source archive. This reproduces the failing
Qt path; the report alone does not reconstruct every Samsung keyboard event.

Qt 5 processes Selection attributes and scrolls to the cursor **inside** an
unfinished text edit block, before updating the preedit and finishing layout.
Android's input context emits empty-preedit Selection events when positioning
the cursor after composition/commit. The embedded prompt editor now removes
these Selection attributes from Qt's event, lets Qt complete commit/preedit
cancellation, and then applies the requested cursor/anchor through QTextCursor
after ensuring the target block layout. Positions are bounded to the document.
Ordinary composition, format and preedit cursor attributes still use Qt's
normal path; read-only fields retain Qt's handling. No prompt text is stripped,
and tag completion and the clipboard format are unchanged.

Regression tests cover positive/negative fields, combined and separate Android
commit/selection events, clearing preedit, reversed selections, stale offsets,
and undo/redo. Existing copy/history/new-document tests remain applicable.
The local diagnostics additionally record `ime.selection-after-layout` with
numeric cursor/anchor values only.

Install over the existing application without uninstalling and retry the user's
history-copy → new-document → paste flow. Physical Samsung acceptance remains
pending; automated checks cannot confirm the device keyboard sequence.

Primary Qt sources:
- https://github.com/qt/qtbase/blob/v5.15.16-lts-lgpl/src/widgets/widgets/qwidgettextcontrol.cpp
- https://github.com/qt/qtbase/blob/v5.15.16-lts-lgpl/src/widgets/widgets/qplaintextedit.cpp
- https://doc.qt.io/qt-5/qinputmethodevent.html
