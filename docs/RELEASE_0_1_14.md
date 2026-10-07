# Android 0.1.14 — ControlNet parity

Reference: the local Python plugin 1.53.0-baron.8, `ui/control.py`,
`model/control.py`, `model/model.py` and `backend/resources.py`.

## Changes

- From Image uses the visible full canvas, excluding the temporary result
  preview, as in Python's `DocumentModel.generate_control_layer`. The chosen
  ControlNet layer remains the destination/conditioning source, not the image
  to preprocess.
- The original preprocessor icon is accessible in every supported map row,
  including a narrow Qwen/Flux 2 panel. Previously the icon was hidden below
  420 pixels and the alternative was inside inaccessible advanced options.
  Keeping this icon visible on narrow panels is an intentional accessibility
  difference from Python. Wide-panel layout and icon are preserved.
- The segmentation From Regions icon is likewise accessible in narrow panels.
- Reference, Style, Composition and Face do not expose a preprocessor, matching
  the original plugin. Unsupported instruction modes remain unavailable.
- Qwen Layered (`qwen_l`) now follows the original edit-model restrictions.
- A normal press on Add Control Layer immediately adds a row, starting with
  Scribble and remembering the last selected mode. Edit-only architectures
  start with Reference. Long press retains the Android shortcut menu for
  choosing a specific mode; the row's mode selector remains available.
- Preprocessor results create a regular editable Krita layer, link to the
  initiating control ID, and do not enter generation history. Buttons and source
  selectors are disabled until the job finishes, without disabling other rows.

## Verification

Regression tests first reproduced missing icons at 320 pixels, incorrect
`qwen_l` support and the wrong image source. Tests cover all 15 public modes at
320/560 pixels on SDXL, Qwen 2.1, Flux 2, Krea 2 and Qwen Layered; request
payloads, row targeting, job locking and error handling. A local HTTP fixture
checks click → prepare → submit → history → download → apply → layer linkage.
Windows Qt and Linux AddressSanitizer suites are run separately. APK verification
checks the final compiled native module, signature, version, public fonts and
matching source archive. Physical Samsung tablet acceptance remains separate.

## Remaining differences

This release does not claim complete parity. The full canvas is preprocessed;
Python's selection-padded preprocessor crop is still absent. Backend-specific
missing-resource messages, GPU control limits, conditional range support,
vector OpenPose editing, Live, Animation and Custom Graph remain incomplete.
See `PARITY_AUDIT_0_1_12.md` for the broader audit and `RELEASE_0_1_13.md` for
connection/backend limitations. Original authorship, channels and service links
remain in the About and Connection settings. The Android port is independent
and is not presented as Acly's endorsed release.
