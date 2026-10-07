# Android 0.1.11: Generate-button crash correction

## Reproduced failure

The native panel retained a QStringBuilder in `BaronPanel::input()` while joining
the positive prompt, negative prompt and style prompt. Krita enables
`QT_USE_QSTRINGBUILDER`, so `auto` retained references to temporary strings rather
than an owning QString. The next statement parsed LoRA tags through those dangling
references, before canvas capture and before any network request.

The new button-to-result regression test reproduced an AddressSanitizer
`stack-use-after-scope` in `qstringbuilder.h`, called from `BaronPanel.cpp:1406`,
`prepare(true)` and the Generate-button click handler. The uncorrected trace is
saved in `build/generate-crash-before.txt`. After materializing an owning QString,
the same test completed successfully (`build/generate-crash-after.txt`).

The tablet version and physical-device crash trace were not available at the time
of diagnosis. This is a reproduced defect in the native generation path; it does
not assert that every possible tablet crash has this cause.

## Changes

- Keep the joined prompt as an owning QString before scanning LoRA tags.
- Keep workflow-log and imported-tag paths as owning QString values; the same
  temporary-lifetime mistake existed in those optional paths.
- Raise the Android version to 0.1.11, code 5050411, retaining the app ID and signing
  key so installation is an update over earlier Baron releases.

## Regression coverage

The test signs in against a local HTTP fixture, selects a model, enters long
positive/negative prompts including a LoRA tag, and clicks the real Generate
button. It checks canvas dimensions, authenticated workflow preparation, queue
submission, result download, saved history and canvas-preview invocation.
It then generates with a selection mask and refines at 43 percent strength,
checking the uploaded source/mask and that an existing selected preview survives.
Workflow logging is enabled so its corrected path is exercised as well.

The complete native Qt suite passed 53 checks with AddressSanitizer on Linux,
with two optional external-integration checks skipped. The build uses Krita's
three string-builder definitions, so the lifetime behavior matches the plugin
compiled into the APK.

These requests use synthetic responses; they do not spend credits or exercise a
live diffusion server. The CanvasHost is a test double, so real Krita projection,
selection and layer rendering on the Samsung tablet remain device acceptance
checks. Public APKs retain the original Android Qt libraries.

Install the new APK as an update; do not uninstall the app or clear its data.
No website service restart is needed for this client-side correction.
