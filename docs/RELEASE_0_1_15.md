# Android 0.1.15 — generate without an imported style

Selecting a model without a saved style sent `style_options: null`: a non-const
QJsonObject lookup created the previously absent member during LoRA parsing.
The compiler rejected it with `Invalid style options` before preparing a graph.

The native request now explicitly contains an empty object, and prompt parsing
uses non-mutating lookup. The server accepts null optional style options as the
default model preset, maintaining compatibility with Android 0.1.13/0.1.14.
Arrays, strings, numbers and booleans remain invalid; style option contents keep
their existing validation.

Regression coverage checks a real native generation-button request with no
style and identical compiled graphs for omitted/null options on Qwen and Krea.
The deployed Orchestrion compiler and direct-Comfy adapter compiler receive only
the null compatibility patch, with backups and hashes checked. They launch the
compiler per request, so this change needs no server restart. Live preparation
is verified without submitting a GPU job.

This APK includes the ControlNet work documented in `RELEASE_0_1_14.md`.
Physical Samsung tablet verification and complete plugin parity remain open.
