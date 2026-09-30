# Verification status — 2026-09-30

## Confirmed

- Independent private GitHub repository: `Darxarz/krita-baron-android`.
- Full Android Krita dependencies fetched, SDK/NDK installed in WSL2.
- Full Krita and native Baron module compiled, linked and packaged for ARM64.
- Final Qt native client tests: 16 passed on Linux and 16 on Windows.
- Full-screen gallery search/selection and collapsed-section tests passed on Linux.
- Server compiler: 5 tests passed, covering Qwen 2.1 / Krea 2 generation, edit with
  reference and selection, person isolation, upscale, validation and Russian CLI
  transport. No GPU inference was executed.
- Isolated website: 54 tests passed across integration, native preparation, proxy
  guards and authentication security; production site was not changed.
- Ruff checks/format and Pyright passed for the new Python compiler/tools/tests.
- All 429 vendored engine files match the reviewed SHA-256 snapshot.
- Android Keystore helper compiled against the Android SDK.
- APK signature verifies using v2; package `org.krita.baron.debug`, Android API 24+,
  target SDK 35. APK includes the current native gallery/client and Keystore DEX
  class and contains no Python interpreter or PyKrita/PyQt runtime.

## APK

File: `artifacts/krita-baron-android-arm64-v0.1.0-preview.apk`.
Size: 181,135,182 bytes (about 173 MiB).
SHA-256: `f0a2344f5281d8ca5ba4b5670e8003730c7eff3c32471e699d2c5b401be66c76`.
APK files are excluded from Git; the repository contains the verification JSON.

This is a debug-signed development build based on Krita 5.4.0-prealpha.
It installs beside official Krita. Open its Settings → Dockers menu and select
AI Diffusion · Baron Edition. Android launch and that menu interaction have not
been verified on a physical tablet yet.

## In progress / unverified

- Starting the APK on an Android device, painting, selections, undo and layer masks.
- Android Keystore runtime and return from external browser login.
- Production native API, billed inference and actual output quality.
- Complete desktop feature parity and complete translations beyond Russian/English.

`baron_preview` screenshots prove only the native panel layout. They are not
screenshots from Android Krita. No tablet is connected to ADB on this laptop.
