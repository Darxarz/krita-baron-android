# Contributing

Please open an issue before starting a large workspace port. Include the app/plugin
version, platform/device, backend, model family and minimal reproduction steps for
bugs. Remove account credentials from logs. Preserve Acly/Krita attribution and
third-party licenses; keep changes focused and document runtime limitations.

The Android source has Qt client tests, Python compiler tests and a separate full
Krita APK build. The desktop source follows AGENTS.md and its Ruff, Pyright and
pytest checks. Automated tests and physical-device validation are separate gates.

Check the current parity report before picking a porting task:
https://github.com/Darxarz/krita-baron-android/blob/main/docs/CURRENT_PARITY.md
