# Android 0.1.17 — prompt input and visual parity

The reported Android keyboard regression occurs when tag completion opens
QCompleter's separate Qt popup. This release uses a non-focusable QListView
inside the existing Krita window on Android, backed by the same completion model.
It never calls QCompleter::complete on the embedded path. Desktop popup behavior
remains available.

Typing, tapping a tag or LoRA, moving through suggestions, and dismissing the list
retain prompt focus. Android Back/Escape first dismisses the list. Clicking
elsewhere dismisses it without consuming the click. The list stays inside the
available window area and avoids a reported keyboard rectangle; when there is
not enough room for a row it remains hidden.

Uncommitted IME preedit text is left to the keyboard. Completion waits for commit,
so an autocorrect/composition sequence cannot insert a tag into unfinished text.
This intentionally respects the Android IME rather than displaying suggestions
for an uncommitted word. The committed prompt remains editable throughout.

Tag rows now display category colors and the dataset/frequency metadata like the
Python plugin. Android rows have a larger touch target. Tag parentheses and LoRA
syntax retain the existing escaping/trigger handling.

## Visual changes included

- A hidden page no longer determines the width of the full dock. A regression
  checks that the generation view fits 320 pixels and that its core controls stay
  inside the dock.
- Includes the previously unreleased UI pass documented in
  `UI_PARITY_2026_10_04.md`: prompt/form margins, grouped generation button,
  five-percent strength step, theme-aware history actions, and higher-resolution
  history thumbnails/favorites.
- Manual selection feathering preferences were not modified. No automatic color
  matching or selection-mask algorithm was changed in this UI pass.

## Validation boundaries

Focused Windows and Linux ASAN tests exercise the embedded path explicitly:
focus retention during typing and selection, IME preedit/commit, dismissal, LoRA
grammar, and a 320-pixel dock. These tests do not emulate Samsung's software
keyboard. No physical Android device is attached; actual IME visibility must be
verified on the user's tablet.

Full test, APK, signature, source and public download results are recorded after
the build completes. This release does not claim complete plugin parity.
