# Krita Baron Edition 0.1.8

This is an unofficial modified build of Krita 5.4.0-prealpha,
upstream revision `96434e2e71aae3a509506518bf83c312b634ac66`.
The visible application name and Android launcher icon distinguish this build.
The temporary geometric B icon will be replaced by the user's final artwork.
The Android application ID remains `org.krita.baron.debug` for update compatibility.
Upstream copyright, license, resource identifiers and file formats are preserved.

Changes:

- Feather Selection is available as a button and context-menu action in the floating
  selection actions bar. It invokes Krita's standard Feather Selection operation,
  radius dialog and undo handling.
- Model gallery includes folder navigation and folder search, search across names,
  paths and trigger words, architecture/type filters, name/folder sorting and
  persistent favorites. Existing thumbnail loading and cache remain in use.
- Selecting a LoRA inserts `<lora:folder/name:strength>` into the active main/regional
  prompt without `.safetensors`, then adds its trigger words. Selecting it again updates
  the existing prompt tag. The Styles settings still manage their own LoRA list.
  Explicit style LoRAs already named in prompt tags are excluded from the request list.
- Prompt autocomplete supports the four original plugin tag CSVs. Danbooru and e621
  are enabled by default; Interface settings allow enabling/disabling each dataset.
  Completion uses category colors, frequency ordering, substring matching, escaped
  parentheses, LoRA filename completion and trigger words. Text-change handling also
  covers Android input methods.
- Public APK contains 96 original OFL/Apache font files and per-family licenses;
  30 families contain all Russian upper/lowercase letters, including Yo. See
  [CURATED_FONTS.md](CURATED_FONTS.md). No Windows font bundle is included.

Build with `scripts/build-public-apk.sh` after generating the public bundle with
`scripts/curate-public-fonts.py`. `scripts/stage-public-fonts.py` validates original
font/license hashes and rejects a personal bundle. `patches/` records the two upstream
source changes; patch application checks before modifying existing code.

Verification records are in `artifacts/` and `build/test-018-final.txt`.
Desktop Qt and static APK checks are separate from physical Android runtime acceptance.
Generation against the live service and Samsung tablet acceptance still require device
testing. This release does not claim complete parity for Live, Animation or Custom Graph.

Dependencies used for this build:

- Krita dependency management: `7eab9569cb65fe8422224fc1de644a169fd8b0f4`,
  https://invent.kde.org/packaging/krita-deps-management
- Krita CI utilities: `ed9f250c4de8382b8f8352739c88be75342f0bf8`,
  https://invent.kde.org/packaging/krita-ci-utilities
- Android NDK r27d, Qt 5.15.7. See build scripts for setup and pinned QtWebSockets source.
- Font repository revision: `9710da1eacb3be272583c3224dcb70f9da6eadbb`.

The release source archive includes the modified Krita source tree, the Baron overlay,
build scripts, bundled engine source and public font files/licenses. The standalone font
catalogue includes searchable real font specimens and original licensing information.
