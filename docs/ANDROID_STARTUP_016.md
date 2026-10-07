# Android 0.1.6 startup correction

## Confirmed defect

Krita's build defines `QT_USE_QSTRINGBUILDER`, `QT_USE_FAST_CONCATENATION`
and `QT_USE_FAST_OPERATOR_PLUS`. The standalone preview originally did not.
`PluginUi::icon` assigned a concatenation expression to `const auto stem`.
With those definitions, this retained a lazy builder referring to temporary
strings after their lifetime ended, instead of an owning QString. Loading the
first workspace icon could dereference invalid memory while creating the dock.

The Android diagnostic module logged a SIGSEGV immediately after
`icon workspace-generation`. Rebuilding the Linux preview with Krita's three
definitions reproduced a startup SIGSEGV. The previous-settings test failed
on `generate-1` with signal 11 before the correction.

## Correction

- Materialize the icon path as QString before using it.
- Materialize the history heading as QString and price cache key as QByteArray;
  both had the same lifetime pattern.
- Build preview and tests with the actual Krita string-builder definitions.
- Test icons in dark/light themes and startup with 12 combinations of saved
  workspace and strength settings.
- Guard palette/mode updates until panel controls exist, restore the connection
  after construction, and verify the image view before accessing its canvas.

## Verification

Linux and Windows Qt suites: 43 passed, 0 failed, 1 optional live test skipped
on each platform. Final Android package verification is recorded alongside
the delivery. No workflow/server compiler or Python plugin change is needed
for this native C++ defect.

The test emulator is x86_64 with an ARM translation layer. Both the user-working
0.1.4 and 0.1.5 initially hit an unsupported scalar ARM conversion instruction
in their identical QtCore library. Only the emulator's installed QtCore was
adapted to equivalent vector instructions to continue investigating. Published
APKs retain the original QtCore. The temporary signal probe was also installed
only in the emulator and removed from the build overlay before final packaging.
These adaptations are not physical Samsung runtime verification.

The final APK was installed over the earlier package in that emulator. After
the original QtCore translation workaround, it passed the former first-icon
SIGSEGV and encountered another translator-only SIGILL: `0x5ea1b863`
(scalar ARM float-to-integer conversion) in the unchanged Krita pigment
library. Full emulator startup is therefore NOT confirmed. No tablet is
connected. Physical Galaxy Tab S8+ startup remains a user acceptance check.

Final APK: 181585054 bytes, SHA-256
`0b66735fc8f08164c11b907cab39f372bea537cc334420e7cdaa9010151be4c4`.
Package `org.krita.baron.debug`, code 5050406, name 5.4.0-baron.0.1.6.
Its signature matches 0.1.4 and 0.1.5. The APK native code section matches
the final compiled module, diagnostic signal probe absent, QtCore identical
to the prior APK, Python absent. A complete public download matches the hash.

[Download APK](https://orchestrion.su/baron-updates/releases/krita-baron-android-arm64-v0.1.6.apk).
Install as an update over the previous Baron app; no uninstall or data reset
is required. This correction requires no website service restart.

Model preview thumbnails and the remaining plugin UI parity work are separate
open tasks; this release does not claim to complete them.
