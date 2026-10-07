# Build instructions

## Android ARM64

The full Krita build runs on Linux. On the development laptop it uses WSL2,
Ubuntu 24.04. The stable migration uses `/opt/baron-stable-5.3.4`, separate from
the previous `/opt/baron-android` development build. The Windows source worktree
is mounted as `/mnt/d/krita-baron-stable`.

Prerequisites:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build git curl unzip aria2 \
  python3-venv openjdk-17-jdk-headless gettext pkg-config libgl1-mesa-dev \
  libvulkan-dev ccache qtbase5-dev qttools5-dev-tools
```

Run from this repository:

```sh
export BARON_BUILD_ROOT=/opt/baron-stable-5.3.4
bash scripts/download-ndk.sh
bash scripts/install-android-sdk.sh
bash scripts/bootstrap-android.sh
bash scripts/build-android.sh
```

The build requires substantial disk space and downloads KDE's Android dependency
packages. First SDK setup accepts the Android SDK licences. Read those terms
before running it on a new machine.

Pinned Krita source: `e7e52a72ed37ecaf9ffaa2fab836b7c2f5539d1f` (upstream `v6.0.4`).
Built with Qt 5, these official release sources produce stable Krita `5.3.4`.
Qt 6 builds use the `6.0.4` version; this Android edition explicitly uses Qt 5.
NDK r27d, Android native API 24, Java 17; KDE dependencies include
patched Qt 5.15.7 and KF5 5.101.0. The package builder uses Gradle 8.13 / AGP 8.12.
The stable-base APK uses compile/target SDK 36 and native/minimum API 24.
This was verified from the produced APK rather than inferred from CI settings.

Dependency tooling used for the initial build:

- krita-deps-management: `7eab9569cb65fe8422224fc1de644a169fd8b0f4`
- krita-ci-utilities: `ed9f250c4de8382b8f8352739c88be75342f0bf8`

The current bootstrap downloads dependency-tooling master. For a reproducible
rebuild, check out these revisions before fetching the dependency environment.
The download cache can change upstream; preserve the actual `_install` and cache
if exact binary reproducibility is needed.

The overlay changes applicationId to `org.krita.baron`. The first debug APK adds
`.debug`, so it can coexist with official Krita. Java's existing `org.krita`
namespace is preserved. PythonLibrary discovery is disabled for this APK build.
Android signing keys and APKs are excluded from Git.
Updates retain the original Baron package identity and signer. The Android
version code continues increasing even though the Krita base changes from 5.4
development sources to stable 5.3.4. Do not uninstall the existing Baron app.

Only use `install-overlay.py` on a clean isolated full Krita checkout. The
`refresh-overlay.py` script updates the previously installed Baron files only.
Do not run the old Python-probe lab's reduced-source build scripts here.

Upstream build guidance:
[Krita Android build guide](https://docs.krita.org/en/untranslatable_pages/building/build_krita_for_android.html).

## UI preview and tests

```sh
cmake -S . -B build/preview -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build/preview --parallel 2
ctest --test-dir build/preview --output-on-failure
QT_QPA_PLATFORM=offscreen build/preview/baron_preview --screenshot panel.png
```

Requires desktop Qt5 Widgets/Network/Test 5.15. Windows builds use MSVC 2022
and Qt 5.15.2 (see `scripts/build-windows-preview.cmd`). The preview uses a fake
white canvas and saves applied results to PNG; it does not validate Krita's
document adapter, Android lifecycle or Android Keystore.

Previous Python experiments remain in
`C:\Users\Darx\Documents\krita for android`. Tooling and a separate Krita source
worktree also remain in `D:\krita-baron-android`. Neither is replaced by this repo.
