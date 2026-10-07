# Krita Baron Edition 0.1.36

- Rebased the full Android application on the official Krita 5.3.4 release
  sources: upstream tag v6.0.4, commit e7e52a72ed37ecaf9ffaa2fab836b7c2f5539d1f,
  explicitly built with Qt 5. This replaces the June 5.4 pre-alpha snapshot.
- Preserved all existing Baron 0.1.35 native client and server features. The
  migration does not claim to complete the remaining Python-plugin parity gaps.
- Adapted the application-name and selection-feather patches to the stable base.
- Added the goat chewing a paintbrush on a rainbow background as the launcher
  icon, with five density variants and an adaptive foreground within safe bounds.
- Retained 96 redistributable font families and their license files.
- Fixed Android-to-desktop .kra interoperability: persistent float settings keep
  their decimal form, and current generation/edit prompts are written into the
  Python-compatible document state. The companion Baron .15 desktop plugin also
  reads older Android files and recovers their prompt banks.
- Preserved org.krita.baron.debug and the existing APK signer. Version code
  5050436 supersedes 5050435; versionName is 5.3.4-baron.0.1.36. Android updates
  compare versionCode, so this is an update rather than an app downgrade.
- Build scripts reject an incorrect upstream revision rather than silently
  packaging a different Krita version or resetting an existing source checkout.

Krita 5.3.4 is a stable upstream release; the Android platform remains beta and
this independently modified edition remains a preview. Physical Samsung tablet
acceptance is separate from compilation, automated tests, and package checks.

## Verification

- Fresh full ARM64 Krita build on the pinned stable source, with the latest Baron
  module rebuilt after the history fix. No pre-alpha Krita output was retained
  in the staging prefix.
- Native Qt suite: 126 passed, 0 failed, 1 optional live-thumbnail test skipped.
- Python engine/Comfy bridge tests: 25 passed.
- Cross-runtime fixture loaded and re-saved with the previous strict Python
  reader, including float settings, document prompt, PNG alpha and batch history.
- Companion desktop checks: 505 passed, 111 skipped; 7 local-Comfy tests cannot
  initialize because their separate tests/server installation is absent.
  Pyright reports 0 errors; all Python files are formatted. Existing unrelated
  Ruff findings remain in the desktop baseline.
- APK contents verified: Krita 5.3.4, Baron 0.1.36, the goat launcher images,
  96 public font families, packaged native features, and no Python interpreter.
- APK signature verified and matches the previous Baron certificate; package
  org.krita.baron.debug, versionCode 5050436, compile/target SDK 36, minimum API 24.
  ZIP alignment verified with 16 KB page alignment.

No Samsung tablet is currently attached for an on-device launch/generation test.
